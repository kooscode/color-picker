
#ifndef pickerError_hpp
#define pickerError_hpp

#include <iostream>
#include <exception>

using namespace std;

class pickerError: public exception
{
    public:
        pickerError(string errmsg);
        virtual const char* what() const throw();
    private:
        string _errmsg;
};

#endif /* pickerError_hpp */
