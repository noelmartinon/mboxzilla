/*
    BSD 2-Clause License

    Copyright (c) 2017, Noël Martinon
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __COMMON_HPP
#define __COMMON_HPP

#include <string>
#include <iostream>        // string::iterator
#include <vector>
#include <sstream>
#include <iomanip>         //setfill
#include <algorithm>    // search
#include <cstring>         //strerror
#include <map>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <errno.h>
#include <cmath>         // floor
#include <dirent.h>     // dirent, opendir

/// Cross platform sleep functions
#ifdef _WIN32
    #include <windows.h>
    inline void usleep(int usec) {
        return Sleep(usec/1000);
    }
#elif __linux__
    #include <unistd.h>
    inline int Sleep(int sleepMs) {
        return usleep(sleepMs * 1000); 
    }
#endif

/// Helper functor for case insensitive find
/**
 * Based on code from
 * http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find
 *
 * templated version of my_equal so it could work with both char and wchar_t
 */
template<typename charT>
struct my_equal {
    my_equal( const std::locale& loc ) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }
private:
    const std::locale& loc_;
};

/// find substring (case insensitive)
/**
 * Based on code from
 * http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find
 */
template<typename T>
int ci_find_substr( const T& str1, const T& str2, const std::locale& loc = std::locale() )
{
    typename T::const_iterator it = std::search( str1.begin(), str1.end(),
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
    if ( it != str1.end() ) return it - str1.begin();
    else return -1; // not found
}

/// find substring from index (case insensitive)
/**
 * Based on code above
 */
// find substring from index (case insensitive)
template<typename T>
int ci_find_substr( const T& str1, const T& str2, size_t index, const std::locale& loc = std::locale() )
{
    typename T::const_iterator it = std::search( str1.begin()+index, str1.end(),
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
    if ( it != str1.end() ) return it - str1.begin();
    else return -1; // not found
}

template<class C, class T>
auto contains(const C& v, const T& x)
-> decltype(end(v), true)
{
    return end(v) != std::find(begin(v), end(v), x);
}

bool NoCaseLess(const std::string &a, const std::string &b);
bool FileExists(const std::string& file);
bool DirectoryExists(const std::string& directory);
bool ListDirectoryContents(std::vector<std::string>& vList, const std::string directory, bool bGetFiles=true, bool bGetDirectories=true);
bool ListAllSubDirectories(std::vector<std::string>& vList, const std::string directory);
std::string PrintMD5(std::string str);
size_t  offset(std::vector<char> &haystack, std::string const &str, size_t index=0);
size_t  ci_offset(std::vector<char> &haystack, std::string const &str, size_t index=0);
void split(const std::string& s, char c, std::vector<std::string>& v, bool allowEmptyString=true);
int get_month_num( std::string name );
int get_day_index( std::string name );
time_t getDiffTime(std::string strdate1, std::string strdate2="");
bool createPath( std::string path, mode_t mode=0777 );
int wildcmp(const char *wild, char *string);
std::string trim(const std::string& str, const std::string& whitespace = " \t");
std::string reduce(const std::string& str, const std::string& fill = " ", const std::string& whitespace = " \t");
bool match(const char *first, const char * second);
void str_replace(std::string& str, const std::string& from, const std::string& to);
size_t count_needle(std::string const &haystack, std::string const &needle);
size_t count_needle(std::vector<std::string> const &haystack, std::string const &needle);
std::string path_dusting (const std::string path);
std::string bytes_convert(double bytes);
bool is_number(const std::string& s);
bool is_asctime(std::string s, bool strict = true);

#endif
