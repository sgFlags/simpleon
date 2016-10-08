#include "simpleon.hpp"
#include <iostream>
#include <stdexcept>

using namespace simpleon;
using namespace std;

int main() {
    IParser * parser = CreateSimpleONParser();

    string line;
    int line_num = 1;
    while (getline(cin, line)) {
        try {
            parser->ParseLine(line);
        }
        catch (const exception &e) {
            cerr << "Parsing error at line" << line_num << ": " << e.what() << endl;
        }
    }

    Dump(cout, parser->Extract());
    cout << endl;

    return 0;
}