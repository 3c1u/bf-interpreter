#include <iostream>
#include <fstream>
#include <string>
#include <functional>
#include <vector>
#include <string.h>

#define MEM_SIZE  30000
#define RET_SIZE  100
#define BF_TOKENS "+-[]><.,"

template <typename T>
class Maybe {
private:
  T    just;
  bool nothing = true;
public:
  Maybe() {}
  ~Maybe() {}
  void operator()(T &a) {
    if(nothing) {
      just    = a;
      nothing = false;
    }
  }
  bool operator~() {
    return !nothing;
  }
  operator T() {
    return just;
  }
  operator bool() {
    return !nothing;
  }
};

template <typename T>
Maybe<T> Just(T a) {
  Maybe<T> o;
  o(a);
  return o;
}

template <typename T>
Maybe<T> Nothing() {
  Maybe<T> o;
  return o;
}

struct bf_code;
struct bf_statement;

enum bf_type {
  BF_NULL = 0x00,
  BF_CODE = 0x01,
  BF_STATEMENT,
  BF_VALUE,
  BF_POINTER,
  BF_MUL,
  BF_ZERO,
  BF_LOOP,
  BF_PRINT,
  BF_GETCH
};

struct bf_statement {
  bf_type        type      = BF_NULL;
  int            increment = 0;
  int            offset    = 0;
  bf_code       *code      = NULL;
  bf_statement  *next      = NULL; //for BF_MUL
};

struct bf_code {
  bf_code      *next;
  bf_statement  stat;
};

void releaseTree(bf_code *code) {
  if(code->stat.type == BF_LOOP)
    releaseTree(code->stat.code);
  if(code->next)
    releaseTree(code->next);
  if(code)
    free(code);
}

struct ParserStream {
  const char *zero;
  const char *current;
};

template <typename T>
using Parser = std::function<Maybe<T> (ParserStream*)>;

template <typename T>
Maybe<T> parse (const Parser<T> &p, const char &code) {
  ParserStream stream;
  Maybe<T>     t;
  stream.zero    = &code;
  stream.current = &code;
  t = p(&stream);
  return t;
}

template <typename T>
const Parser<T> operator||(const Parser<T> &a, const Parser<T> &b) {
    return [=](ParserStream *stream) {
        ParserStream _b  = *stream;
        Maybe<T>     res = a(stream);
        if(~res) return res;
        else {
          *stream = _b;
          return b(stream);
        }
    };
}

Parser<char>
skipExcept(const char * temp) {
  int len = strlen(temp);
  return [=](ParserStream *stream) {
    char ch = *stream->current;
    while (ch) {
      for(int i = 0; i < len; i++) {
        if(temp[i] == ch) {
          //stream->current++;
          return Just(ch);
        }
      }
      stream->current++;
      ch = *stream->current;
    }
    return Nothing<char>();
  };
}

Parser<char>
satisfy(const std::function <bool (char)> &f) {
  return [=](ParserStream *stream) {
    char ch = *stream->current;
    if (f(ch)) {
      stream->current++;
      return Just<char>(ch);
    } else {
      return Nothing<char>();
    }
  };
}

template <typename T>
Maybe<T> revert(ParserStream *stream, ParserStream streamBackup) {
  *stream = streamBackup;
  return Nothing<T>();
}

template <typename T>
Maybe<T> success(ParserStream *stream, ParserStream streamBackup, T t) {
  *stream = streamBackup;
  return Just(t);
}

/*bf parser*/
extern Parser<bf_statement> value_increment;
extern Parser<bf_statement> pointer_increment;
extern Parser<bf_statement> nullo;
extern Parser<bf_statement> zero;
extern Parser<bf_code*>     codebf;

Parser<char>  tokenParser = [] (ParserStream *stream) {
  //token ::= /[+-.,><\[\]]/
  Maybe<char> input = skipExcept(BF_TOKENS)(stream);
  stream->current++;
  return input;
};

Parser<bf_statement> value_increment = [] (ParserStream *stream) {
  //value ::= '+' | '-'
  bf_statement  val;
  Maybe<char>   input       = Nothing<char>();
  ParserStream  streamBackup;

  //set type to BF_VALUE
  val.type = BF_VALUE;

  //backup for revert
  streamBackup = *stream;

  //if input is empty
  if(!~(input = tokenParser(stream))) return Nothing<bf_statement>();
  if      (char(input) == '+') val.increment++;
  else if (char(input) == '-') val.increment--;
  else                         return revert<bf_statement>(stream, streamBackup);
  streamBackup = *stream;
  input        = tokenParser(stream);
  
  while(~input) {
    if      (char(input) == '+') val.increment++;
    else if (char(input) == '-') val.increment--;
    else                         return success<bf_statement>(stream, streamBackup, val);
    streamBackup = *stream;
    input        = tokenParser(stream);
  }

  return Just(val);
};

Parser<bf_statement> pointer_increment = [] (ParserStream *stream) {
  //pointer ::= '>' | '<'
  bf_statement  val;
  Maybe<char>   input         = Nothing<char>();
  ParserStream  streamBackup;

  //set type to BF_POINTER
  val.type = BF_POINTER;

  //skip until symbols come
  (skipExcept(BF_TOKENS))(stream);
  streamBackup  = *stream;

  //if input is empty
  if(!~(input = tokenParser(stream))) return Nothing<bf_statement>();

  if      (char(input) == '>') val.increment++;
  else if (char(input) == '<') val.increment--;
  else                         return revert<bf_statement>(stream, streamBackup);
  streamBackup = *stream;
  input        = tokenParser(stream);
  
  while(~input) {
    if      (char(input) == '>') val.increment++;
    else if (char(input) == '<') val.increment--;
    else                         return success<bf_statement>(stream, streamBackup, val);
    streamBackup = *stream;
    input        = tokenParser(stream);
  }

  return Just(val);
};

Parser<bf_statement> nullo     = [] (ParserStream* stream) {
  //nullo ::= "[]"
  bf_statement val;
  Maybe<char>   input         = Nothing<char>();
  ParserStream  streamBackup;
  //set type to BF_ZERO
  val.type = BF_ZERO;

  //set backup
  streamBackup  = *stream;

  //if input is empty
  if(!~(input = tokenParser(stream))) return Nothing<bf_statement>();
  if(char(input) == '[') {
    input = tokenParser(stream);
    if(char(input) == ']') return Just<bf_statement>(val);
  }
  return revert<bf_statement>(stream, streamBackup);
};

Parser<bf_statement> zero     = [] (ParserStream* stream) {
  //zero ::= [-]
  bf_statement  val;
  Maybe<char>   input         = Nothing<char>();
  ParserStream  streamBackup;
  //set type to BF_ZERO
  val.type = BF_ZERO;

  //set backup
  streamBackup  = *stream;

  //if input is empty
  if(!~(input = tokenParser(stream))) return Nothing<bf_statement>();
  if(char(input) == '[') {
    input = tokenParser(stream);
    if(char(input) == '-') {
      input = tokenParser(stream);
      if(char(input) == ']') return Just<bf_statement>(val);
    }
  }
  return revert<bf_statement>(stream, streamBackup);
};

Parser<bf_statement> loop_block = [] (ParserStream* stream) {
  //loop ::= '[' code ']'
  bf_statement  val;
  Maybe<char>   input         = Nothing<char>();
  ParserStream  streamBackup;

  //set type to BF_LOOP
  val.type = BF_LOOP;

  //skip until symbols come
  (skipExcept(BF_TOKENS))(stream);
  streamBackup  = *stream;

  //if input is empty
  if(!~(input = tokenParser(stream))) return Nothing<bf_statement>();

  if(char(input) == '[') {
    Maybe<bf_code*> loopcode = codebf(stream);
    if(!~loopcode) {
      std::cout << "Empty loop code." << std::endl;
      exit(1);
      return Nothing<bf_statement>();
    }
    input = tokenParser(stream);
    if(char(input) == ']') {
      val.code = loopcode;
      return Just(val);
    }
    std::cout << "Expected ], unexpected " 
              << char(input)
              << " at "
              << int(stream->current - stream->zero)
              << std::endl;
    
    releaseTree(loopcode);
    exit(1);
  }

  return revert<bf_statement>(stream, streamBackup);
};

Parser<bf_statement> io = [] (ParserStream* stream) {
  //io ::= '.' | ','
  Parser<char> p1 = satisfy([] (char c) { return c == '.'; });
  Parser<char> p2 = satisfy([] (char c) { return c == ','; });
  Maybe<char>  c;
  
  (skipExcept(BF_TOKENS))(stream);

  c = p1(stream);
  if(~c) {
    bf_statement s;
    s.type = BF_PRINT;
    return Just(s);
  }
  c  = p2(stream);
  if(~c) {
    bf_statement s;
    s.type = BF_GETCH;
    return Just(s);
  }
  return Nothing<bf_statement>();
};

Parser<bf_statement> statement = [] (ParserStream* stream) {
  //statement  ::= value | pointer | null | mul | zero | loop_block
  Parser<bf_statement> p = value_increment 
                            || pointer_increment 
                            || nullo 
                            || zero 
                            || io 
                            || loop_block;
  return p(stream);
};

Parser<bf_code*> singleCode = [] (ParserStream* stream) {
  Maybe<bf_statement> s = statement(stream);
  if(~s) {
    bf_code *aCode = new bf_code;
    aCode->stat = bf_statement(s);
    aCode->next = NULL;
    return Just(aCode);
  }
  return Nothing<bf_code*>();
};

/*Parser<bf_code*>*/
Parser<bf_code*> codebf = [] (ParserStream* stream) {
  //code ::= statement code | statement
  //     ::= code+

  Maybe<bf_code*> aCode = singleCode(stream);
  ParserStream    bak   = *stream;
  if(!~aCode) return Nothing<bf_code*>();
  bf_code        *begin = (bf_code*)(aCode);
  bf_code        *now   = begin;
    while(~(aCode = singleCode(stream))) {
      now->next = aCode;
      now       = aCode;
      bak       = *stream;
    }
    
    return success(stream, bak, begin);
};

unsigned char _ptr[30000] = {0};
int           _i          = 0;

void runcode(bf_code *code) {
  if(code == NULL) return;

  bf_statement stat = code->stat;

  switch(stat.type) {
    case BF_NULL:
      //std::cout << "ptr[i] = 0;" << std::endl;
      break;
    case BF_VALUE:
      _ptr[_i] += stat.increment;
      break;
    case BF_POINTER:
      _i += stat.increment;
      break;
    case BF_MUL:
      // std::cout << "// BF_MUL not implemented" << std::endl;
    case BF_ZERO:
      _ptr[_i] = 0;
      break;
    case BF_LOOP:
      while(_ptr[_i]) {
        runcode(stat.code);
      }
      break;
    case BF_PRINT:
      printf("%c", _ptr[_i]);
      break;
    case BF_GETCH:
      _ptr[_i] = getchar();
      break;
    default:
      std::cout << "// not impleneted" << std::endl;
      break;
  }
  runcode(code->next);
}

int main(int argc,
        const char **argv,
        const char **envp) {
  const char              *filename = argv[1];
  std::ifstream            fs(filename);
  std::string              dat((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
  const char              *code = dat.c_str();
  Maybe<bf_code*>          res  = parse(codebf, *code);

  if(~res)  runcode(res);
  else      std::cout << "No code generated. Abort" << std::endl;
  releaseTree(res);

  return 0;
}