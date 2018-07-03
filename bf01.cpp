#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

unsigned char _ptr[30000] = {0};
unsigned int  _i = 0;

void
runBrainfuck(const char* code) {
  int         retStack[100] = {0};
  int         i             = 0;
  int         pos           = 0;
  
  while(code[i]) {
    switch(code[i]) {
      case '+':
        _ptr[_i]++;
        break;
      case '-':
        _ptr[_i]--;
        break;
      case '>':
        _i++;
        break;
      case '<':
        _i--;
        break;
      case '[':
        if(_ptr[_i]) {
          retStack[pos++] = i;
        } else {
          int _parenCount = 1;
          do {
            i++;
            if     (code[i] == '[') _parenCount++;
            else if(code[i] == ']') _parenCount--;
          } while(_parenCount && !!(code[i]));
        }
        break;
      case ']':
        //back to loop
        pos--;
        i = retStack[pos] - 1;
        break;
      case '.':
        std::cout << _ptr[_i];
        break;
      case ',':
        _ptr[_i] = getchar();
        break;
      default:
        break;
    }
    i++;
  }
}

int main(int argc,
        const char **argv,
        const char **envp) {
  const char              *filename = argv[1];
  std::ifstream            fs(filename);
  std::string              dat((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
  const char              *code = dat.c_str();
  runBrainfuck(code);

  return 0;
}
