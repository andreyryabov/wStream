#include <string>
#include <vector>

std::string base64_encode(unsigned char const* , unsigned int len);
std::vector<uint8_t>  base64_decode(std::string const& s);

