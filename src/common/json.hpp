// SPDX-License-Identifier: MIT
#pragma once
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace tas {
struct Json {
  enum class Kind { Null, Boolean, Number, String, Array, Object } kind=Kind::Null;
  bool boolean=false;
  double number=0;
  std::string string;
  std::vector<Json> array;
  std::map<std::string,Json> object;
  const Json& at(const std::string& k) const {
    auto it=object.find(k);
    if(kind!=Kind::Object || it==object.end()) throw std::runtime_error("Missing JSON key: "+k);
    return it->second;
  }
  const Json* get(const std::string& k) const {
    if(kind!=Kind::Object)return nullptr;
    auto it=object.find(k);
    return it==object.end()?nullptr:&it->second;
  }
  std::string str() const { if(kind!=Kind::String)throw std::runtime_error("Expected JSON string");return string; }
  double num() const { if(kind!=Kind::Number || !std::isfinite(number))throw std::runtime_error("Expected finite JSON number");return number; }
};
class JsonParser {
  const std::string& s; size_t pos=0; int depth=0;
  void ws(){while(pos<s.size() && std::isspace(static_cast<unsigned char>(s[pos])))++pos;}
  char peek(){ws();return pos<s.size()?s[pos]:'\0';}
  bool eat(char c){if(peek()==c){++pos;return true;}return false;}
  void require(char c){if(!eat(c))throw std::runtime_error(std::string("Expected '")+c+"' at "+std::to_string(pos));}
  std::string parseString(){
    require('"');std::string r;
    while(pos<s.size()){
      char c=s[pos++]; if(c=='"')return r;
      if(static_cast<unsigned char>(c)<32)throw std::runtime_error("Control character in string");
      if(c=='\\'){
        if(pos==s.size())throw std::runtime_error("Truncated escape");
        char e=s[pos++];
        switch(e){case '"':case '\\':case '/':r+=e;break;case 'n':r+='\n';break;case 'r':r+='\r';break;case 't':r+='\t';break;case 'b':r+='\b';break;case 'f':r+='\f';break;
          case 'u':{
            if(pos+4>s.size())throw std::runtime_error("Invalid unicode escape");
            unsigned value=0;
            for(int i=0;i<4;i++){
              char h=s[pos++];value*=16;
              if(h>='0'&&h<='9')value+=h-'0';else if(h>='a'&&h<='f')value+=h-'a'+10;
              else if(h>='A'&&h<='F')value+=h-'A'+10;else throw std::runtime_error("Invalid unicode hex");
            }
            // Convert basic BMP to UTF-8. Surrogate pairs are not needed for preset keys.
            if(value<0x80)r+=(char)value;
            else if(value<0x800){r+=(char)(0xc0|(value>>6));r+=(char)(0x80|(value&63));}
            else {r+=(char)(0xe0|(value>>12));r+=(char)(0x80|((value>>6)&63));r+=(char)(0x80|(value&63));}
            break;
          }
          default:throw std::runtime_error("Unknown escape");
        }
      }else r+=c;
    }
    throw std::runtime_error("Unterminated JSON string");
  }
  Json value(){
    if(++depth>80)throw std::runtime_error("JSON nesting too deep");
    Json j;char c=peek();
    if(c=='{'){
      ++pos;j.kind=Json::Kind::Object;
      if(!eat('}'))do{auto k=parseString();require(':');j.object.emplace(std::move(k),value());if(eat('}'))break;require(',');}while(true);
    }else if(c=='['){
      ++pos;j.kind=Json::Kind::Array;
      if(!eat(']'))do{j.array.push_back(value());if(eat(']'))break;require(',');}while(true);
    }else if(c=='"'){j.kind=Json::Kind::String;j.string=parseString();}
    else if(c=='t'&&s.compare(pos,4,"true")==0){pos+=4;j.kind=Json::Kind::Boolean;j.boolean=true;}
    else if(c=='f'&&s.compare(pos,5,"false")==0){pos+=5;j.kind=Json::Kind::Boolean;}
    else if(c=='n'&&s.compare(pos,4,"null")==0){pos+=4;}
    else if(c=='-'||std::isdigit(static_cast<unsigned char>(c))){
      const char* begin=s.c_str()+pos;char* end=nullptr;
      j.number=std::strtod(begin,&end);if(end==begin || !std::isfinite(j.number))throw std::runtime_error("Invalid JSON number");
      pos=static_cast<size_t>(end-s.c_str());j.kind=Json::Kind::Number;
    }else throw std::runtime_error("Invalid JSON token at "+std::to_string(pos));
    --depth;return j;
  }
 public:
  explicit JsonParser(const std::string& source):s(source){}
  Json parse(){auto x=value();ws();if(pos!=s.size())throw std::runtime_error("Trailing JSON content");return x;}
};
}
