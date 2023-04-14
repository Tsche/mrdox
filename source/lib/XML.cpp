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

#include "XML.hpp"

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// XMLGenerator
//
//------------------------------------------------

bool
XMLGenerator::
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
    if(R.error(ec, "open a stream for '", fileName, "'"))
        return false;

    if(! corpus.canonicalize(R))
        return false;
    Writer w(corpus, config, R);
    w.write(os);
    return true;
}

bool
XMLGenerator::
buildString(
    std::string& dest,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);

    if(! corpus.canonicalize(R))
        return false;
    Writer w(corpus, config, R);
    w.write(os);
    return true;
}

//------------------------------------------------
//
// Writer
//
//------------------------------------------------

XMLGenerator::
Writer::
Writer(
    Corpus const& corpus,
    Config const& config,
    Reporter& R) noexcept
    : RecursiveWriter(corpus, config, R)
{
}

//------------------------------------------------

void
XMLGenerator::
Writer::
beginDoc()
{
    os() <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
}

void
XMLGenerator::
Writer::
endDoc()
{
    os() <<
        "</mrdox>\n";
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeAllSymbols(
    std::vector<AllSymbol> const& list)
{
    openTag("all");
    adjustNesting(1);
    for(auto const& I : list)
        writeTag("symbol", {
            { "name", I.fqName },
            { "tag", I.symbolType },
            { I.id } });
    adjustNesting(-1);
    closeTag("all");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
beginNamespace(
    NamespaceInfo const& I)
{
    openTag("namespace", {
        { "name", I.Name },
        { I.USR }
        });
}

void
XMLGenerator::
Writer::
writeNamespace(
    NamespaceInfo const& I)
{
    writeInfo(I);
}

void
XMLGenerator::
Writer::
endNamespace(
    NamespaceInfo const& I)
{
    closeTag("namespace");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
beginRecord(
    RecordInfo const& I)
{
    llvm::StringRef tag =
        clang::TypeWithKeyword::getTagTypeKindName(I.TagType);
    openTag(tag, {
        { "name", I.Name },
        { I.USR }
        });
}

void
XMLGenerator::
Writer::
writeRecord(
    RecordInfo const& I)
{
    writeInfo(I);
    writeSymbol(I);
    for(auto const& t : I.Bases)
        writeBaseRecord(t);
    // VFALCO data members?
}

void
XMLGenerator::
Writer::
endRecord(
    RecordInfo const& I)
{
    llvm::StringRef tag =
        clang::TypeWithKeyword::getTagTypeKindName(I.TagType);
    closeTag(tag);
}

//------------------------------------------------

void
XMLGenerator::
Writer::
beginFunction(
    FunctionInfo const& I)
{
    openTag("function", {
        { "name", I.Name },
        { I.Access },
        { I.USR }
        });
}

void
XMLGenerator::
Writer::
writeFunction(
    FunctionInfo const& I)
{
    writeInfo(I);
    writeSymbol(I);
    writeTag("return", {
        { "name", I.ReturnType.Type.Name },
        { I.ReturnType.Type.USR }
        });
    for(auto const& J : I.Params)
        writeParam(J);
    if(I.Template)
        for(TemplateParamInfo const& J : I.Template->Params)
            writeTemplateParam(J);
}

void
XMLGenerator::
Writer::
endFunction(
    FunctionInfo const& I)
{
    closeTag("function");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeEnum(
    EnumInfo const& I)
{
    openTag("enum", {
        { "name", I.Name },
        { I.USR }
        });
    adjustNesting(1);
    writeInfo(I);
    for(auto const& v : I.Members)
        writeTag("element", {
            { "name", v.Name },
            { "value", v.Value },
            });
    adjustNesting(-1);
    closeTag("enum");
}

void
XMLGenerator::
Writer::
writeTypedef(
    TypedefInfo const& I)
{
    openTag("typedef", {
        { "name", I.Name },
        { I.USR }
        });
    adjustNesting(1);
    writeInfo(I);
    writeSymbol(I);
    if(I.Underlying.Type.USR != EmptySID)
        writeTagLine("qualusr", toBase64(I.Underlying.Type.USR));
    adjustNesting(-1);
    closeTag("typedef");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeInfo(
    Info const& I)
{
}

void
XMLGenerator::
Writer::
writeSymbol(
    SymbolInfo const& I)
{
    if(I.DefLoc)
        writeLocation(*I.DefLoc, true);
    for(auto const& loc : I.Loc)
        writeLocation(loc, false);
}

void
XMLGenerator::
Writer::
writeLocation(
    Location const& loc,
    bool def)
{
    writeTag("file", {
        { "path", loc.Filename },
        { "line", std::to_string(loc.LineNumber) },
        { "class", "def", def } });
}

void
XMLGenerator::
Writer::
writeBaseRecord(
    BaseRecordInfo const& I)
{
    writeTag("base", {
        { "name", I.Name },
        { I.Access },
        { "modifier", "virtual", I.IsVirtual },
        { I.USR } });
    if(! corpus_.exists(I.USR))
    {
        // e,g. for std::true_type
        return;
    }
}

void
XMLGenerator::
Writer::
writeParam(
    FieldTypeInfo const& I)
{
    writeTag("param", {
        { "name", I.Name },
        { "default", I.DefaultValue, ! I.DefaultValue.empty() },
        { "type", I.Type.Name },
        { I.Type.USR }
        });
}

void
XMLGenerator::
Writer::
writeTemplateParam(
    TemplateParamInfo const& I)
{
    writeTag(
        "tparam", {
        { "decl", I.Contents }
        });
}

//------------------------------------------------

void
XMLGenerator::
Writer::
openTag(
    llvm::StringRef tag)
{
    indent() << '<' << tag << ">\n";
}

void
XMLGenerator::
Writer::
openTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    indent() << '<' << tag;
    writeAttrs(attrs);
    os() << ">\n";
}

void
XMLGenerator::
Writer::
closeTag(
    llvm::StringRef tag)
{
    indent() << "</" << tag << ">\n";
}

void
XMLGenerator::
Writer::
writeTag(
    llvm::StringRef tag)
{
    indent() << "<" << tag << "/>\n";
}

void
XMLGenerator::
Writer::
writeTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    indent() << "<" << tag;
    writeAttrs(attrs);
    os() << "/>\n";
}

void
XMLGenerator::
Writer::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value)
{
    indent() <<
        "<" << tag << ">" <<
        escape(value) <<
        "</" << tag << ">"
        "\n";
}

void
XMLGenerator::
Writer::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value,
    Attrs attrs)
{
    indent() << "<" << tag;
    writeAttrs(attrs);
    os() << ">" <<
        escape(value) <<
        "</" << tag << ">"
        "\n";
}

void
XMLGenerator::
Writer::
writeAttrs(
    Attrs attrs)
{
    for(auto const& attr : attrs)
        if(attr.pred)
            os() <<
                ' ' << attr.name << '=' <<
                "\"" << escape(attr.value) << "\"";
}

//------------------------------------------------

std::string
XMLGenerator::
Writer::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
XMLGenerator::
Writer::
toString(
    InfoType it) noexcept
{
    switch(it)
    {
    case InfoType::IT_default:    return "default";
    case InfoType::IT_namespace:  return "namespace";
    case InfoType::IT_record:     return "record";
    case InfoType::IT_function:   return "function";
    case InfoType::IT_enum:       return "enum";
    case InfoType::IT_typedef:    return "typedef";
    default:
        llvm_unreachable("unknown InfoType");
    }
}

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<XMLGenerator>();
}

} // mrdox
} // clang
