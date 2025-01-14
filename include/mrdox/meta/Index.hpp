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

#ifndef MRDOX_META_INDEX_HPP
#define MRDOX_META_INDEX_HPP

#include <mrdox/meta/Info.hpp>
#include <mrdox/meta/Reference.hpp>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <vector>

namespace clang {
namespace mrdox {

struct Index : Reference
{
    Index() = default;

    Index(
        llvm::StringRef Name)
        : Reference(SymbolID(), Name)
    {
    }

    Index(
        llvm::StringRef Name,
        llvm::StringRef JumpToSection)
        : Reference(SymbolID(), Name)
        , JumpToSection(JumpToSection)
    {
    }

    Index(
        SymbolID USR,
        llvm::StringRef Name,
        InfoType IT,
        llvm::StringRef Path)
        : Reference(USR, Name, IT, Path)
    {
    }

    // This is used to look for a USR in a vector of Indexes using std::find
    bool operator==(const SymbolID& Other) const
    {
        return USR == Other;
    }

    bool operator<(const Index& Other) const;

    llvm::Optional<llvm::SmallString<16>> JumpToSection;
    std::vector<Index> Children;

    void sort();
};

} // mrdox
} // clang

#endif
