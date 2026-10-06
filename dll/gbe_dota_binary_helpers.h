#ifndef __INCLUDED_GBE_DOTA_BINARY_HELPERS_H__
#define __INCLUDED_GBE_DOTA_BINARY_HELPERS_H__

#include <cstring>
#include <string>

template <class T>
inline void ser_var(std::string &buf, const T &input)
{
    buf.append(reinterpret_cast<const char *>(&input), sizeof(T));
}

template <class T>
inline T deser_var(const char *&p)
{
    T output;
    memcpy(&output, p, sizeof(T));
    p += sizeof(T);
    return output;
}

#endif // __INCLUDED_GBE_DOTA_BINARY_HELPERS_H__
