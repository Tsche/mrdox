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

#ifndef MRDOX_META_SYMBOL_HPP
#define MRDOX_META_SYMBOL_HPP

#include <mrdox/meta/Info.hpp>
#include <mrdox/meta/Location.hpp>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>

namespace clang {
namespace mrdox {

/** Base class for Info that have source locations.
*/
struct SymbolInfo : Info
{
    llvm::Optional<Location> DefLoc;     // Location where this decl is defined.
    llvm::SmallVector<Location, 2> Loc; // Locations where this decl is declared.

    //--------------------------------------------

    explicit
    SymbolInfo(
        InfoType IT,
        SymbolID USR = SymbolID(),
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef Path = llvm::StringRef())
        : Info(IT, USR, Name, Path)
    {
    }

    void merge(SymbolInfo&& I);
};

} // mrdox
} // clang

#endif
