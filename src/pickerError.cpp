#include "pickerError.hpp"

pickerError::pickerError(string errmsg)
{
    _errmsg = errmsg;
}

const char* pickerError::what() const throw()
{
    return _errmsg.c_str();
}

