//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Asciidoc.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/format/OverloadSet.hpp>
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// AsciidocGenerator
//
//------------------------------------------------

bool
AsciidocGenerator::
build(
    llvm::StringRef rootPath,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    namespace path = llvm::sys::path;

#if 0
    // Track which directories we already tried to create.
    llvm::StringSet<> CreatedDirs;

    // Collect all output by file name and create the necessary directories.
    llvm::StringMap<std::vector<mrdox::Info*>> FileToInfos;
    for (const auto& Group : corpus.InfoMap)
    {
        mrdox::Info* Info = Group.getValue().get();

        llvm::SmallString<128> Path;
        llvm::sys::path::native(RootDir, Path);
        llvm::sys::path::append(Path, Info->getRelativeFilePath(""));
        if (!CreatedDirs.contains(Path)) {
            if (std::error_code Err = llvm::sys::fs::create_directories(Path);
                Err != std::error_code()) {
                return llvm::createStringError(Err, "Failed to create directory '%s'.",
                    Path.c_str());
            }
            CreatedDirs.insert(Path);
        }

        llvm::sys::path::append(Path, Info->getFileBaseName() + ".adoc");
        FileToInfos[Path].push_back(Info);
    }

    for (const auto& Group : FileToInfos) {
        std::error_code FileErr;
        llvm::raw_fd_ostream InfoOS(Group.getKey(), FileErr,
            llvm::sys::fs::OF_None);
        if (FileErr) {
            return llvm::createStringError(FileErr, "Error opening file '%s'",
                Group.getKey().str().c_str());
        }

        for (const auto& Info : Group.getValue()) {
            if (llvm::Error Err = generateDocForInfo(Info, InfoOS, config)) {
                return Err;
            }
        }
    }
    return true;
#else
    llvm::SmallString<0> fileName(rootPath);
    path::append(fileName, "reference.adoc");
    return buildOne(fileName, corpus, config, R);
#endif
}

bool
AsciidocGenerator::
buildOne(
    llvm::StringRef fileName,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;

    std::error_code ec;
    llvm::raw_fd_ostream os(
        fileName,
        ec,
        fs::CD_CreateAlways,
        fs::FA_Write,
        fs::OF_None);
    if(R.error(ec, "open the stream for '", fileName, "'"))
        return false;

    Writer w(os, corpus, config, R);
    w.beginFile();
    w.visitAllSymbols();
    w.endFile();
    return ! os.has_error();
}

bool
AsciidocGenerator::
buildString(
    std::string& dest,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);

    Writer w(os, corpus, config, R);
    w.beginFile();
    w.visitAllSymbols();
    w.endFile();
    return true;
}

//------------------------------------------------
//
// FlatWriter
//
//------------------------------------------------

struct AsciidocGenerator::Writer::
    FormalParam
{
    FieldTypeInfo const& I;
    Writer& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        FormalParam const& t)
    {
        t.w.writeFormalParam(t, os);
        return os;
    }
};

struct AsciidocGenerator::Writer::
    TypeName
{
    TypeInfo const& I;
    Writer& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        TypeName const& t)
    {
        t.w.writeTypeName(t, os);
        return os;
    }
};

//------------------------------------------------

AsciidocGenerator::
Writer::
Writer(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Config const& config,
    Reporter& R) noexcept
    : FlatWriter(os, corpus, config, R)
{
}

void
AsciidocGenerator::
Writer::
beginFile()
{
    openTitle("Reference");
    os_ <<
        ":role: mrdox\n";
}

void
AsciidocGenerator::
Writer::
endFile()
{
    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeFormalParam(
    FormalParam const& t,
    llvm::raw_ostream& os)
{
    auto const& I = t.I;
    os << I.Type.Name << ' ' << I.Name;
}

auto
AsciidocGenerator::
Writer::
formalParam(
    FieldTypeInfo const& t) ->
        FormalParam
{
    return FormalParam{ t, *this };
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeRecord(
    RecordInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc.getBrief());

    // Synopsis
    openSection("Synopsis");

    writeLocation(I);

    // Declaration
    os_ <<
        "\n" <<
        "[,cpp]\n"
        "----\n" <<
        toString(I.TagType) << " " << I.Name;
    if(! I.Bases.empty())
    {
        os_ << "\n    : ";
        writeBase(I.Bases[0]);
        for(std::size_t i = 1; i < I.Bases.size(); ++i)
        {
            os_ << "\n    , ";
            writeBase(I.Bases[i]);
        }
    }
    os_ <<
        ";\n"
        "----\n";
    closeSection();

    // Description
    writeDescription(I.javadoc.getBlocks());

    writeMemberTypes(
        "Data Members",
        I.Members,
        AccessSpecifier::AS_public);

    writeOverloadSet(
        "Member Functions",
        makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_public;
            }));

    writeMemberTypes(
        "Protected Data Members",
        I.Members,
        AccessSpecifier::AS_protected);

    writeOverloadSet(
        "Protected Member Functions",
        makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_protected;
            }));

    writeMemberTypes(
        "Private Data Members",
        I.Members,
        AccessSpecifier::AS_private);

    writeOverloadSet(
        "Private Member Functions",
        makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_private;
            }));

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeFunction(
    FunctionInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc.getBrief());

    // Synopsis
    openSection("Synopsis");

    writeLocation(I);

    os_ <<
        "\n"
        "[,cpp]\n"
        "----\n";
    if(! I.Params.empty())
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "(\n"
            "    " << formalParam(I.Params[0]);
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            os_ << ",\n"
                "    " << formalParam(I.Params[i]);
        }
        os_ << ");\n";
    }
    else
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "();" << "\n";
    }
    os_ <<
        "----\n";
    closeSection();

    // Description
    writeDescription(I.javadoc.getBlocks());

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeEnum(
    EnumInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc.getBrief());

    writeLocation(I);

    // Description
    writeDescription(I.javadoc.getBlocks());

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeTypedef(
    TypedefInfo const& I)
{
    openSection(I.Name);

    // Brief
    writeBrief(I.javadoc.getBrief());

    writeLocation(I);

    // Description
    writeDescription(I.javadoc.getBlocks());

    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeLocation(Location const& loc)
{
    os_ <<
        "\n"
        "`#include <" << loc.Filename << ">`\n";
}

void
AsciidocGenerator::
Writer::
writeBase(
    BaseRecordInfo const& I)
{
    os_ << clang::getAccessSpelling(I.Access) << " " << I.Name;
}

void
AsciidocGenerator::
Writer::
writeOverloadSet(
    llvm::StringRef sectionName,
    std::vector<OverloadSet> const& list)
{
    if(list.empty())
        return;
    openSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(auto const& J : list)
    {
        os_ <<
            "|`" << J.name << "`\n" <<
            "|";
        for(auto const& K : J.list)
            writeBrief(K->javadoc.getBrief());
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

void
AsciidocGenerator::
Writer::
writeMemberTypes(
    llvm::StringRef sectionName,
    llvm::SmallVectorImpl<MemberTypeInfo> const& list,
    AccessSpecifier access)
{
return;
    if(list.empty())
        return;

    auto it = list.begin();
    for(;;)
    {
        if(it == list.end())
            return;
        if(it->Access == access)
            break;
        ++it;
    }
    openSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(;it != list.end(); ++it)
    {
        if(it->Access != access)
            continue;
        os_ <<
            "|`" << it->Name << "`\n" <<
            "|";
        //os_ << it->javadoc.brief << "\n";
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeBrief(
    Javadoc::Paragraph const* node)
{
    if(! node)
        return;
    if(node->empty())
        return;
    os_ << '\n';
    writeNode(*node);
}

void
AsciidocGenerator::
Writer::
writeLocation(
    SymbolInfo const& I)
{
    Location const* loc = nullptr;
    if(I.DefLoc.has_value())
        loc = &*I.DefLoc;
    else if(! I.Loc.empty())
        loc = &I.Loc[0];
    os_ <<
        "\n"
        "#include <"
        "file:///" << loc->Filename <<
        "[" << loc->Filename << "]" <<
        ">\n";
}
    
void
AsciidocGenerator::
Writer::
writeDescription(
    List<Javadoc::Block> const& list)
{
    if(list.empty())
        return;
    os_ << "\n";
    openSection("Description");
    writeNodes(list);
    closeSection();
}

//------------------------------------------------

template<class T>
void
AsciidocGenerator::
Writer::
writeNodes(
    List<T> const& list)
{
    if(list.empty())
        return;
    for(Javadoc::Node const& node : list)
        writeNode(node);
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Node const& node)
{
    switch(node.kind)
    {
    case Javadoc::Kind::text:
        writeNode(static_cast<Javadoc::Text const&>(node));
        return;
    case Javadoc::Kind::styled:
        writeNode(static_cast<Javadoc::StyledText const&>(node));
        return;
#if 0
    case Javadoc::Node::block:
        writeNode(static_cast<Javadoc::Block const&>(node));
        return;
#endif
    case Javadoc::Kind::brief:
    case Javadoc::Kind::paragraph:
        writeNode(static_cast<Javadoc::Paragraph const&>(node));
        return;
    case Javadoc::Kind::admonition:
        writeNode(static_cast<Javadoc::Admonition const&>(node));
        return;
    case Javadoc::Kind::code:
        writeNode(static_cast<Javadoc::Code const&>(node));
        return;
    case Javadoc::Kind::param:
        writeNode(static_cast<Javadoc::Param const&>(node));
        return;
    case Javadoc::Kind::tparam:
        writeNode(static_cast<Javadoc::TParam const&>(node));
        return;
    case Javadoc::Kind::returns:
        writeNode(static_cast<Javadoc::Returns const&>(node));
        return;
    default:
        llvm_unreachable("unknown kind");
    }
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Text const& node)
{
    os_ << node.string << '\n';
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::StyledText const& node)
{
    switch(node.style)
    {
    case Javadoc::Style::bold:
        os_ << '*' << node.string << "*\n";
        break;
    case Javadoc::Style::mono:
        os_ << '`' << node.string << "`\n";
        break;
    case Javadoc::Style::italic:
        os_ << '_' << node.string << "_\n";
        break;
    default:
        os_ << node.string << '\n';
        break;
    }
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Paragraph const& node)
{
    writeNodes(node.children);
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Admonition const& node)
{
    writeNodes(node.children);
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Code const& node)
{
    os_ <<
        "[,cpp]\n"
        "----\n";
    writeNodes(node.children);
    os_ <<
        "----\n";
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Param const& node)
{
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::TParam const& node)
{
}

void
AsciidocGenerator::
Writer::
writeNode(
    Javadoc::Returns const& node)
{
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeTypeName(
    TypeName const& t,
    llvm::raw_ostream& os)
{
    if(t.I.Type.USR == EmptySID)
    {
        os_ << t.I.Type.Name;
        return;
    }
    auto p = corpus_.find<RecordInfo>(t.I.Type.USR);
    if(p != nullptr)
    {
        // VFALCO add namespace qualifiers if I is in
        //        a different namesapce
        os_ << p->Path << "::" << p->Name;
        return;
    }
    auto const& T = t.I.Type;
    os_ << T.Path << "::" << T.Name;
}

auto
AsciidocGenerator::
Writer::
typeName(
    TypeInfo const& t) ->
        TypeName
{
    return TypeName{ t, *this };
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
openTitle(
    llvm::StringRef name)
{
    assert(sect_.level == 0);
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        sect_.markup << ' ' << name << "\n";
}

void
AsciidocGenerator::
Writer::
openSection(
    llvm::StringRef name)
{
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        "\n" <<
        sect_.markup << ' ' << name << "\n";
}

void
AsciidocGenerator::
Writer::
closeSection()
{
    assert(sect_.level > 0);
    if(sect_.level <= 6)
        sect_.markup.pop_back();
    sect_.level--;
}

//------------------------------------------------

Location const&
AsciidocGenerator::
Writer::
getLocation(
    SymbolInfo const& I)
{
    if(I.DefLoc.has_value())
        return *I.DefLoc;
    if(! I.Loc.empty())
        return I.Loc[0];
    static Location const missing{};
    return missing;
}

llvm::StringRef
AsciidocGenerator::
Writer::
toString(TagTypeKind k) noexcept
{
    switch(k)
    {
    case TagTypeKind::TTK_Struct: return "struct";
    case TagTypeKind::TTK_Interface: return "__interface";
    case TagTypeKind::TTK_Union: return "union";
    case TagTypeKind::TTK_Class: return "class";
    case TagTypeKind::TTK_Enum: return "enum";
    default:
        llvm_unreachable("unknown TagTypeKind");
    }
}

//------------------------------------------------

std::unique_ptr<Generator>
makeAsciidocGenerator()
{
    return std::make_unique<AsciidocGenerator>();
}

} // mrdox
} // clang
