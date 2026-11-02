#include "../include/security.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
namespace lms::security {
namespace {
constexpr int iterations=310000;
std::string hex(const unsigned char* data,size_t n){std::ostringstream out;out<<std::hex<<std::setfill('0');for(size_t i=0;i<n;++i)out<<std::setw(2)<<static_cast<unsigned>(data[i]);return out.str();}
std::vector<unsigned char> fromHex(const std::string& input){if(input.size()%2)throw std::invalid_argument("Invalid hash encoding");std::vector<unsigned char> out;out.reserve(input.size()/2);for(size_t i=0;i<input.size();i+=2){auto h=input.substr(i,2);size_t used=0;int value=std::stoi(h,&used,16);if(used!=2)throw std::invalid_argument("Invalid hex");out.push_back(static_cast<unsigned char>(value));}return out;}
}
std::string hashPassword(const std::string& password){
 if(password.size()<10 || password.size()>128)throw std::invalid_argument("Password must contain 10-128 characters");
 std::array<unsigned char,16> salt{};std::array<unsigned char,32> key{};
 if(RAND_bytes(salt.data(),static_cast<int>(salt.size()))!=1)throw std::runtime_error("Salt generation failed");
 if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),salt.data(),static_cast<int>(salt.size()),iterations,EVP_sha256(),static_cast<int>(key.size()),key.data())!=1)throw std::runtime_error("Password hashing failed");
 return "pbkdf2_sha256$"+std::to_string(iterations)+"$"+hex(salt.data(),salt.size())+"$"+hex(key.data(),key.size());
}
bool verifyPassword(const std::string& password,const std::string& encoded){
 try{
  size_t first=encoded.find('$'), second=encoded.find('$',first+1),third=encoded.find('$',second+1);
  if(first==std::string::npos||second==std::string::npos||third==std::string::npos||encoded.substr(0,first)!="pbkdf2_sha256")return false;
  const int rounds=std::stoi(encoded.substr(first+1,second-first-1));
  if(rounds<100000||rounds>1000000)return false;
  auto salt=fromHex(encoded.substr(second+1,third-second-1));auto expected=fromHex(encoded.substr(third+1));
  if(salt.size()!=16||expected.size()!=32||password.size()>128)return false;
  std::array<unsigned char,32> actual{};
  if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),salt.data(),static_cast<int>(salt.size()),rounds,EVP_sha256(),static_cast<int>(actual.size()),actual.data())!=1)return false;
  return CRYPTO_memcmp(actual.data(),expected.data(),actual.size())==0;
 }catch(const std::exception&){return false;}
}
std::string readSecret(const std::string& prompt){
 std::cout<<prompt<<std::flush;
 std::string value;
#ifdef _WIN32
 if(_isatty(_fileno(stdin))){for(;;){int c=_getch();if(c=='\r'||c=='\n')break;if(c==8){if(!value.empty())value.pop_back();}else if(c>=32&&value.size()<129)value.push_back(static_cast<char>(c));}std::cout<<'\n';return value;}
#else
 if(isatty(STDIN_FILENO)){
  termios old{};if(tcgetattr(STDIN_FILENO,&old)==0){termios hidden=old;hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);tcsetattr(STDIN_FILENO,TCSANOW,&hidden);std::getline(std::cin,value);tcsetattr(STDIN_FILENO,TCSANOW,&old);std::cout<<'\n';return value;}
 }
#endif
 std::getline(std::cin,value);return value;
}
}
