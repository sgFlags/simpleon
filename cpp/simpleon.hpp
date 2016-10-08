#ifndef __SIMPLEON__
#define __SIMPLEON__

#include <string>
#include <list>
#include <map>
#include <iostream>
#include <stdexcept>

namespace simpleon {
    class IData {
    public:
        enum Type {
            T_BOOL,
            T_INT,
            T_FLOAT,
            T_STRING,
            T_UQ_STRING,
            T_LIST,
            T_DICT
        };

        virtual Type   GetType() = 0;
        virtual bool   GetBool();
        virtual int    GetInt();
        virtual double GetFloat();
        virtual const std::string & GetString();
        virtual const std::list<IData *> & GetList();
        virtual const std::map<std::string, IData *> & GetDict();
        virtual ~IData() = default;
    };

    class IParser {
    public:
        virtual void ParseLine(const std::string & line) = 0;
        virtual IData * Extract() = 0;
        virtual ~IParser() = default;
    };

    class ParseException : public std::exception {
    private:
        std::string _what;
    public:
        ParseException(const char * w);
        const char * what() const noexcept override;
    };

    IParser * CreateSimpleONParser();
    void Dump(std::ostream & o, IData * d);
}

#endif
