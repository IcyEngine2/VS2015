#define NOMINMAX 1
#define CURL_STATICLIB 1
#define CURL_STRICTER 1

#include "../../libs/curl/curl/curl.h"
#include <string>
#include <algorithm>
#include <vector>
#include <fstream>
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Advapi32")
#pragma comment(lib, "Crypt32")
#if _DEBUG
#pragma comment(lib, "libcurld.lib")
#else

#endif

#define CHECK(X) if (const auto error_ = (X)) { printf("[EMD]: %s", curl_easy_strerror(error_)); return false; }

struct CpMailData
{
    std::string SendTo;
    std::string SendFrom;
    std::string Subject;
    std::string Body;
    std::string File;
};
class CpMail
{
public:
    CpMail(const CpMailData& data);
    ~CpMail();
    static std::string Cp1251ToUtf8(const std::string& str);
    bool Send(const std::string& server, const std::string& login, const std::string& password, bool ssl);
private:
    static std::string Base64(const std::vector<char>& bytes);
    static size_t Callback(void* ptr, size_t size, size_t nmemb, void* userp);
    struct SList
    {
        SList() : mList(nullptr)
        {

        }
        ~SList()
        {
            curl_slist_free_all(mList);
        }
        curl_slist* mList;
    };
private:
    CURL* mCurl;
    std::string mSendFrom;
    std::string mSendTo;
    size_t mCursor;
    std::string mData;
};
CpMail::CpMail(const CpMailData& data) : mCurl(nullptr),mSendFrom(data.SendFrom), mSendTo(data.SendTo), mCursor(0)
{
    tm tm_time;
    auto ms_time = time(nullptr);
    localtime_s(&tm_time, &ms_time);
    char time_str[256];
    auto time_len = strftime(time_str, _countof(time_str), "%a, %d %b %Y %H:%M:%S %z", &tm_time);

    mData += "User-Agent: CrossPro EMD";
    mData += "\r\nDate: " + std::string(time_str, time_len);
    mData += "\r\nFrom: <" + data.SendFrom + ">";
    mData += "\r\nTo: <" + data.SendTo + ">";
    mData += "\r\nCc: <" + data.SendTo + ">";
    mData += "\r\nSubject: " + data.Subject;
    
    std::string fileData;
    std::string fileName;
    if (!data.File.empty())
    {
        std::ifstream input(data.File, std::ios::binary | std::ios::in);
        if (input)
        {
            input.seekg(0, std::ios::end);
            auto fileSize = input.tellg();
            std::vector<char> bytes(fileSize);
            input.seekg(0, std::ios::beg);
            input.read(&bytes[0], fileSize);
            fileData = Base64(bytes);
            auto pos = data.File.size() - 1;
            while (pos > 0 && data.File[pos] != '\\' && data.File[pos] != '/')
                --pos;
            if (data.File[pos] == '\\' || data.File[pos] == '/')
                ++pos;
            fileName = data.File.substr(pos);
        }
        else
        {
            printf("[EMD]: Error opening file \"%s\"\r\n", data.File.data());
        }
    }

    mData += "\r\nMime-Version: 1.0";

    if (!fileData.empty())
    {
        const std::string boundary = std::string("CrossPro_EMD_Boundary_2020"); 
        mData += "\r\nContent-Type: multipart/mixed; boundary=\"" + boundary + "\"";
        mData += "\r\n\r\nThis is a multi-part message in MIME format.";
        mData += "\r\n--" + boundary;
        mData += "\r\nContent-Type: text/plain;charset=utf-8";
        mData += "\r\n\r\n";
        mData += data.Body;
        mData += "\r\n--" + boundary;
        mData += "\r\nContent-Type: application/octet-stream";
        mData += "\r\nContent-Disposition: attachment; filename=\"" + fileName + "\"";
        mData += "\r\nContent-Transfer-Encoding: base64";
        mData += "\r\n\r\n";
        mData += fileData;
        mData += "\r\n--" + boundary;
        mData += "\r\n";
    }
    else
    {
        mData += "\r\nContent-Type:text/plain;charset=utf-8";
        mData += "\r\n\r\n";
        mData += data.Body;
    }
}
CpMail::~CpMail()
{
    if (mCurl)
        curl_easy_cleanup(mCurl);
}
bool CpMail::Send(const std::string& server, const std::string& login, const std::string& password, bool ssl)
{    
    mCursor = 0;
    if (mCurl) 
        curl_easy_cleanup(mCurl);

    mCurl = curl_easy_init();
    if (!mCurl)
    {
        printf("[EMD]: Failed to initialize CURL\r\n");
        return false;
    }
    CHECK(curl_easy_setopt(mCurl, CURLOPT_URL, server.data()));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_USERNAME, login.data()));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_PASSWORD, password.data()));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_READFUNCTION, Callback));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_READDATA, this));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_UPLOAD, 1L));
    CHECK(curl_easy_setopt(mCurl, CURLOPT_MAIL_FROM, mSendFrom.data()));
    if (ssl)
    {
        CHECK(curl_easy_setopt(mCurl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL));
        CHECK(curl_easy_setopt(mCurl, CURLOPT_SSL_VERIFYPEER, 0L));
        CHECK(curl_easy_setopt(mCurl, CURLOPT_SSL_VERIFYHOST, 0L));
    }
    CHECK(curl_easy_setopt(mCurl, CURLOPT_VERBOSE, 1L));
    SList sendTo;
    sendTo.mList = curl_slist_append(nullptr, mSendTo.data());
    CHECK(curl_easy_setopt(mCurl, CURLOPT_MAIL_RCPT, sendTo.mList));
    const auto error = curl_easy_perform(mCurl);
    curl_easy_cleanup(mCurl);
    mCurl = nullptr;
    CHECK(error);
    return true;
}
std::string CpMail::Cp1251ToUtf8(const std::string& input)
{
    std::vector<wchar_t> wide;
    auto wlen = MultiByteToWideChar(1251, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

    if (wlen < 0)
        return "";
    else if (wlen == 0)
        return "";

    wide.resize(wlen);
    MultiByteToWideChar(1251, 0, input.data(), static_cast<int>(input.size()), &wide[0], wlen);

    std::string ustr;
    auto ulen = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, nullptr, 0, nullptr, nullptr);
    
    if (ulen < 0)
        return "";
    else if (ulen == 0)
        return "";
    
    ustr.resize(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, &ustr[0], ulen, nullptr, nullptr);
    return ustr;
}
std::string CpMail::Base64(const std::vector<char>& bytes)
{
    const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string str;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    auto len = bytes.size();
    auto ptr = bytes.data();
    auto i = 0u;
    while (len--)
    {
        char_array_3[i++] = *(ptr++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = (char_array_3[2] & 0x3f);

            for (auto k = 0; k < 4; ++k)
                str += base64_chars[char_array_4[k]];
            i = 0;
        }
    }

    if (i)
    {
        for (auto j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (auto j = 0; j < i + 1; j++)
            str += base64_chars[char_array_4[j]];

        while (i++ < 3)
            str += '=';
    }
    return str;

}
size_t CpMail::Callback(void* ptr, size_t size, size_t nmemb, void* userp)
{
    if (!ptr || !userp)
        return 0;

    const auto mail = static_cast<CpMail*>(userp);
    const auto len = std::min(mail->mData.size() - mail->mCursor, size * nmemb);
    memcpy(ptr, mail->mData.data() + mail->mCursor, len);
    mail->mCursor += len;
    return len;
}
int main()
{
    CHECK(curl_global_init(CURL_GLOBAL_ALL));
    CpMailData data;
    data.SendFrom = "cross-pro@vaz.ru";
    data.SendTo = "user@sdl.ru";
    data.Body = u8"Тестовое Сообщение 2";
    data.Subject = "CrossProTest";
    CpMail mail(data);
    mail.Send("smtps://mail.vaz.ru:465", "cross-pro@vaz.ru", "cx0ihkTA", true);
    curl_global_cleanup();
    return 0;
}