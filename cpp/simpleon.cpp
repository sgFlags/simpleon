#include "simpleon.hpp"

using namespace simpleon;
using namespace std;

string IData_DUMMY_STRING;
list<IData *> IData_DUMMY_LIST;
map<string, IData *> IData_DUMMY_MAP;

bool IData::GetBool() { return false; }
int IData::GetInt() { return 0; }
double IData::GetFloat() { return 0.0; }
const string & IData::GetString() { return IData_DUMMY_STRING; }
const list<IData *> & IData::GetList() { return IData_DUMMY_LIST; }
const map<string, IData *> & IData::GetDict() { return IData_DUMMY_MAP; }

class IntData : public IData {
public:
    int value;
    Type GetType() override { return T_INT; }
    int GetInt() override { return value; } 
};

class BoolData : public IData {
public:
    bool value;
    Type GetType() override { return T_BOOL; }
    bool GetBool() override { return value; } 
};

class DoubleData : public IData {
public:
    double value;
    Type GetType() override { return T_FLOAT; }
    double GetFloat() override { return value; } 
};

class StringData : public IData {
public:
    string value;
    Type GetType() override { return T_STRING; }
    const string & GetString() override { return value; } 
};

class ListData : public IData {
public:
    list<IData *> value;
    Type GetType() override { return T_LIST; }
    const list<IData *> & GetList() override { return value; }
};

class DictData : public IData {
public:
    map<string, IData *> value;
    Type GetType() override { return T_DICT; }
    const map<string, IData *> & GetDict() override { return value; }
};

class SimpleONParser : public IParser {
private:

    enum State {
        STATE_ELEMENT_START,
        STATE_ELEMENT_END,
        STATE_DICT_PRE_KEY,
        STATE_DICT_KEY,
        STATE_DICT_POST_KEY,
        STATE_DICT_VALUE,
        STATE_DICT_POST_VALUE,
        STATE_LIST,
        STATE_QUOTED_STRING,
        STATE_QUOTELESS_STRING,
        STATE_MULTILINE_STRING
    };
    
    string _buf;
    int _readPos;
    vector<string>  _keyStack;
    vector<IData *> _valueStack;
    vector<State>   _stateStack;
    
public:

    SimpleONParser() {
        _readPos = 0;
        _stateStack.push_back(STATE_ELEMENT_START);
    }
    
    void ParseLine(const string & line) {
        if (_stackStack.size() == 0) return;
        
        _buf.append(line);
        ParseBuf();
    }

    void ParseBuf() {
        if (_stateStack.size() == 0) return;
        
        while (_readPos < _buf.size()) {

            auto state = _stateStack.back();
            switch (state) {
            case STATE_ELEMENT_END: {
                auto value = _valueStack.back();
                
                _stateStack.pop_back();
                if (_stateStack.size() == 0) break;
                _valueStack.pop_back();
                
                switch (_stateStack.back()) {
                case STATE_DICT_KEY:
                    _stateStack.back() = STATE_DICT_POST_KEY;
                    _keyStack.push_back(_valueStack.GetString());
                    break;
                    
                case STATE_DICT_VALUE:
                    ((DictData &)_valueStack.back())[_keyStack.back()] = value;
                    _keyStack.pop_back();
                    _stateStack.back() = STATE_DICT_POST_VALUE;
                    break;
                    
                case STATE_LIST:
                    ((ListData &)_valueStack.back()).push_back(value);
                    break;

                default:
                    throw logic_error("invalid state to insert element");
                }
                break;
            }
            case STATE_QUOTED_STRING: {
                break;
            }
            case STATE_MULTILINE_STRING: {
                break;
            }
            case STATE_DICT_PRE_KEY: {
                break;
            }
            case STATE_DICT_KEY: {
                break;
            }
            case STATE_DICT_POST_KEY: {
                break;
            }
            case STATE_DICT_VALUE: {
                break;
            }
            case STATE_DICT_POST_VALUE: {
                break;
            }
            case STATE_LIST: {
                break;
            }
            case STATE_ELEMENT_START: {
                break;
            }
            }
        }
    }
};
