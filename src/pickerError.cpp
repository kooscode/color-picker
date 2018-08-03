//
//  pickerError.cpp
//  color-picker
//
//  Created by Koos du Preez on 12/17/17.
//  Copyright © 2017 TerraClear Inc. All rights reserved.
//

#include "pickerError.hpp"

pickerError::pickerError(string errmsg)
{
    _errmsg = errmsg;
}

const char* pickerError::what() const throw()
{
    return _errmsg.c_str();
}

