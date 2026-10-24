// ==WindhawkMod==
// @id              auto-lyrics-overlay
// @name            Auto Lyrics Overlay
// @description     Hiển thị lời bài hát đồng bộ (Spotify / YouTube / YouTube Music).
// @version         1.5.0
// @author          Tuan_Moi
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lwinhttp -lws2_32 -lgdiplus -lgdi32 -lole32 -luser32 -ladvapi32 -lbcrypt -lruntimeobject -loleaut32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Auto Lyrics Overlay

Mod này là bản chuyển của script Python `AutoLyricsOverlay` sang Windhawk.

## Nguồn bài hát đang phát

* **Spotify (app trên máy):** đọc trực tiếp qua Windows SMTC (System Media Transport Controls),
  không cần extension hay đăng nhập. Lấy được tên bài, ca sĩ, và ảnh bìa.
  Cần Windows 10 1809 trở lên. Spotify web trong trình duyệt KHÔNG được nhận qua đường này.
* **YouTube / YouTube Music(Stupid):** mở một server cục bộ `127.0.0.1:<port>` (mặc định 8765) nhận `POST /nowplaying`
  với JSON `{source:"youtube", title, artist, url, cover_url, position, duration, playing}`
  — giống hệt extension bạn đang dùng với bản Python.
* Nếu Spotify đang phát thì dữ liệu từ YouTube bị bỏ qua (tránh hai nguồn tranh nhau).
  Khi Spotify tạm dừng, YouTube sẽ được nhận trở lại.

## Nguồn lời

Thứ tự: YouTube Music (chỉ với bài từ YouTube, tra theo videoId) -> Zing MP3 -> NhacCuaTui -> NetEase
-> Lyrica -> LRCLIB -> Musixmatch -> LRCLIB++ -> Lyrics.ovh.
(YouTube Music, Zing MP3, NhacCuaTui, NetEase và Musixmatch là API không chính thức; nếu lỗi thì mod tự chuyển sang nguồn kế tiếp.)
Musixmatch dùng token ẩn danh của endpoint desktop, không cần tài khoản; nếu bị giới hạn/captcha thì mod nghỉ 60 giây rồi thử lại.

## Giao diện

* Lời nằm NGAY TRONG taskbar (cửa sổ con của taskbar), nền trong suốt, chữ tự đổi trắng/đen theo
  theme Windows. Dòng hiện tại kéo lên, dòng kế tiếp trượt lên thay thế.
* Chỉnh vị trí bằng `offsetX` / `offsetY` / `width` (nếu bị nút Widgets che thì tăng `offsetX`).
* Chuột phải vào lời: tìm lại lời / ẩn đến bài tiếp theo.
* **Tên bài hát:** trong N giây đầu của mỗi bài, mod hiện tên bài (dòng lớn) và ca sĩ (dòng nhỏ) trước khi hiện lời.
  Đặt `titleSeconds` = 0 để tắt.
* **Lời quá dài:** chọn cách xử lý trong cài đặt `longLines` — cắt bằng "…", thu nhỏ chữ cho vừa một dòng,
  hoặc thu nhỏ nhẹ rồi xuống dòng (tối đa 2 dòng; khi xuống dòng thì tạm ẩn dòng lời kế tiếp nếu không đủ chỗ).

**Lưu ý:** hãy tắt bản Python trước (cùng dùng cổng 8765), nếu không mod sẽ báo không mở được cổng.
Nếu build lỗi liên quan WinRT/linker, thử thêm `-lwindowsapp` vào `@compilerOptions`.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- port: 8765
  $name: Cổng server cục bộ
  $description: Đổi cổng thì extension cũng phải đổi theo.
- spotify: true
  $name: Nhận bài hát từ Spotify (app trên máy)
  $description: Đọc qua Windows SMTC, không cần extension.
- offsetX: 12
  $name: Cách mép trái taskbar (px)
  $description: Tăng lên nếu bị nút Widgets / tin tức che.
- offsetY: 0
  $name: Dịch dọc (px, âm = lên)
- width: 300
  $name: Chiều rộng vùng lời (px)
- fontSize: 11
  $name: Cỡ chữ lời hiện tại (pt)
- longLines: wrap
  $name: Khi một dòng lời quá dài
  $options:
  - ellipsis: Cắt bớt bằng dấu …
  - shrink: Thu nhỏ chữ cho vừa một dòng
  - wrap: Thu nhỏ nhẹ rồi xuống dòng (tối đa 2 dòng)
- titleSeconds: 4
  $name: Hiện tên bài hát trước khi hiện lời (giây)
  $description: Tên bài + ca sĩ hiện trong N giây đầu của mỗi bài. Đặt 0 để tắt.
- showCover: true
  $name: Hiện ảnh bìa nhỏ
- showNext: true
  $name: Hiện dòng lời kế tiếp
- animate: true
  $name: Hiệu ứng kéo lên khi đổi lời
*/
// ==/WindhawkModSettings==

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <objidl.h>
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>

#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

// ---------------------------------------------------------------------------
// Settings & globals
// ---------------------------------------------------------------------------

struct Settings {
    int port = 8765;
    int fontSize = 11;
    int width = 300;
    int offsetX = 12;
    int offsetY = 0;
    bool spotify = true;
    bool showCover = true;
    bool showNext = true;
    bool animate = true;
    int titleSeconds = 4;
    int longMode = 2;  // LM_*
} g_s;

enum { LM_ELLIPSIS = 0, LM_SHRINK = 1, LM_WRAP = 2 };

static const char* LYRICA_API = "https://wilooper-lyrica.hf.space/lyrics/";
static const char* LRCLIBPP_API = "https://lrclibplusplus.vercel.app/api/lyrics";

static std::mutex g_zingMtx;
static std::string g_zingCookie;

static std::atomic<bool> g_running{false};
static std::atomic<bool> g_serverFailed{false};
static std::atomic<int> g_workers{0};
static std::atomic<int> g_clients{0};
static std::atomic<HWND> g_hwnd{nullptr};
static std::thread g_mainThread;

typedef std::vector<std::pair<double, std::string>> Lines;

struct NowPlaying {
    bool valid = false;
    std::string source;  // "youtube" | "spotify"
    std::string title, artist, url, cover;
    double pos = 0, dur = 0;
    bool playing = false;
    ULONGLONG recvTick = 0;
};
static std::mutex g_stateMtx;
static NowPlaying g_np;

// Ảnh bìa Spotify (lấy từ SMTC) — thread SMTC ghi, thread UI đọc.
struct SpotCover {
    std::string key;
    std::vector<BYTE> data;
};
static std::mutex g_spotCoverMtx;
static SpotCover g_spotCover;

struct LyricsResult {
    std::string status;
    Lines lines;
};
static std::mutex g_cacheMtx;
static std::map<std::pair<std::string, std::string>, LyricsResult> g_cache;

#define WM_LYRICS_DONE (WM_APP + 1)
#define WM_COVER_DONE (WM_APP + 2)

struct LyricsMsg {
    unsigned token;
    LyricsResult res;
};
struct CoverMsg {
    unsigned token;
    std::vector<BYTE> data;
};

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

static std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string Lower(std::string s) {
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

static std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') b++;
    while (e > b && (unsigned char)s[e - 1] <= ' ') e--;
    return s.substr(b, e - b);
}

static std::string Trunc(std::string s, size_t n) {
    if (s.size() <= n) return s;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    s.resize(n);
    return s;
}

static std::string UrlEnc(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            o += (char)c;
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 15];
        }
    }
    return o;
}

static void AppendUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) {
        o += (char)cp;
    } else if (cp < 0x800) {
        o += (char)(0xC0 | (cp >> 6));
        o += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        o += (char)(0xE0 | (cp >> 12));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    } else {
        o += (char)(0xF0 | (cp >> 18));
        o += (char)(0x80 | ((cp >> 12) & 0x3F));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    }
}

static bool HttpsHost(const std::string& u, std::string& host) {
    if (u.compare(0, 8, "https://") != 0) return false;
    size_t e = u.find_first_of("/?#", 8);
    std::string a = u.substr(8, e == std::string::npos ? std::string::npos : e - 8);
    if (a.empty() || a.find_first_of("@:\\ ") != std::string::npos) return false;
    host = Lower(a);
    return true;
}

static bool InList(const std::string& v, const char* const* list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (v == list[i]) return true;
    return false;
}

static const char* const PAGE_HOSTS[] = {"www.youtube.com", "music.youtube.com"};
static bool EndsWithStr(const std::string& s, const char* suf) {
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

// Ảnh bìa chỉ được tải từ các domain của YouTube / Google.
static bool IsCoverHost(const std::string& h) {
    return h == "i.ytimg.com" || h == "img.youtube.com" || h == "i9.ytimg.com" || EndsWithStr(h, ".googleusercontent.com") ||
           EndsWithStr(h, ".ggpht.com");
}

// ---------------------------------------------------------------------------
// Minimal JSON parser
// ---------------------------------------------------------------------------

struct JV {
    enum Type { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JV> a;
    std::vector<std::string> keys;
    std::vector<JV> vals;

    const JV* get(const char* k) const {
        if (t != Obj) return nullptr;
        for (size_t i = 0; i < keys.size(); i++)
            if (keys[i] == k) return &vals[i];
        return nullptr;
    }
    std::string str(const char* k) const {
        const JV* v = get(k);
        return (v && v->t == Str) ? v->s : std::string();
    }
    double num(const char* k, double d) const {
        const JV* v = get(k);
        return (v && v->t == Num && std::isfinite(v->n)) ? v->n : d;
    }
};

struct JP {
    const char* p;
    const char* e;
    int depth = 0;

    void ws() {
        while (p < e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    }
    bool hex4(unsigned& v) {
        if (e - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    bool str(std::string& out) {
        p++;  // opening quote
        while (p < e) {
            char c = *p++;
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (p >= e) return false;
            char d = *p++;
            switch (d) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2;
                        unsigned lo;
                        if (!hex4(lo)) return false;
                        if (lo >= 0xDC00 && lo < 0xE000)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: out += d; break;
            }
        }
        return false;
    }
    bool lit(const char* w) {
        size_t n = strlen(w);
        if ((size_t)(e - p) >= n && memcmp(p, w, n) == 0) {
            p += n;
            return true;
        }
        return false;
    }
    bool parse(JV& v) {
        ws();
        if (p >= e || depth > 120) return false;
        char c = *p;
        if (c == '{') {
            depth++;
            p++;
            v.t = JV::Obj;
            ws();
            if (p < e && *p == '}') {
                p++;
                depth--;
                return true;
            }
            while (true) {
                ws();
                std::string k;
                if (p >= e || *p != '"' || !str(k)) return false;
                ws();
                if (p >= e || *p != ':') return false;
                p++;
                JV x;
                if (!parse(x)) return false;
                v.keys.push_back(std::move(k));
                v.vals.push_back(std::move(x));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; break; }
                return false;
            }
            depth--;
            return true;
        }
        if (c == '[') {
            depth++;
            p++;
            v.t = JV::Arr;
            ws();
            if (p < e && *p == ']') {
                p++;
                depth--;
                return true;
            }
            while (true) {
                JV x;
                if (!parse(x)) return false;
                v.a.push_back(std::move(x));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; break; }
                return false;
            }
            depth--;
            return true;
        }
        if (c == '"') {
            v.t = JV::Str;
            return str(v.s);
        }
        if (lit("true")) { v.t = JV::Bool; v.b = true; return true; }
        if (lit("false")) { v.t = JV::Bool; v.b = false; return true; }
        if (lit("null")) { v.t = JV::Null; return true; }
        const char* s = p;
        while (p < e && (isdigit((unsigned char)*p) || *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E')) p++;
        if (p == s) return false;
        std::string tmp(s, p);
        v.t = JV::Num;
        v.n = strtod(tmp.c_str(), nullptr);
        return true;
    }
};

static bool ParseJson(const std::string& s, JV& out) {
    JP jp{s.data(), s.data() + s.size()};
    if (!jp.parse(out)) return false;
    jp.ws();
    return jp.p == jp.e;
}

// ---------------------------------------------------------------------------
// LRC parsing
// ---------------------------------------------------------------------------

static Lines ParseLrc(const std::string& text) {
    Lines out;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string line = Trim(text.substr(i, j - i));
        i = j + 1;

        std::vector<double> stamps;
        size_t pos = 0;
        while (pos < line.size() && line[pos] == '[') {
            size_t q = pos + 1, d = q;
            while (d < line.size() && isdigit((unsigned char)line[d])) d++;
            if (d == q || d - q > 2 || d >= line.size() || line[d] != ':') break;
            int mm = atoi(line.substr(q, d - q).c_str());
            q = d + 1;
            d = q;
            while (d < line.size() && isdigit((unsigned char)line[d])) d++;
            if (d - q != 2) break;
            int ss = atoi(line.substr(q, 2).c_str());
            double frac = 0;
            if (d < line.size() && (line[d] == '.' || line[d] == ':')) {
                size_t f = d + 1, g = f;
                while (g < line.size() && isdigit((unsigned char)line[g])) g++;
                if (g == f || g - f > 3) break;
                frac = atoi(line.substr(f, g - f).c_str()) / std::pow(10.0, (double)(g - f));
                d = g;
            }
            if (d >= line.size() || line[d] != ']') break;
            stamps.push_back(mm * 60.0 + ss + frac);
            pos = d + 1;
        }
        if (stamps.empty()) continue;
        std::string txt = Trim(line.substr(pos));
        for (double s : stamps) out.emplace_back(s, txt);
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
                         return a.first < b.first;
                     });
    return out;
}

// ---------------------------------------------------------------------------
// WinHTTP
// ---------------------------------------------------------------------------

struct WH {
    HINTERNET h;
    explicit WH(HINTERNET x = nullptr) : h(x) {}
    ~WH() { if (h) WinHttpCloseHandle(h); }
};

static bool HttpFetch(const wchar_t* method, const std::string& url, const std::string& body,
                      std::string& out, size_t maxBytes, bool allowRedirect, bool wantImage,
                      const std::wstring& extraHdrs = L"",
                      const wchar_t* contentType = L"application/json") {
    std::wstring wurl = Widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return false;

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    // HTTP thường chỉ được phép với đúng một host (API di động của NhacCuaTui).
    bool plainOk = (uc.nScheme == INTERNET_SCHEME_HTTP && host == L"api.m.nhaccuatui.com");
    if (uc.nScheme != INTERNET_SCHEME_HTTPS && !plainOk) return false;

    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    bool zing = (host == L"zingmp3.vn");
    WH session(WinHttpOpen((zing || host == L"music.youtube.com" || host == L"apic-desktop.musixmatch.com") ? L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
                                : L"AutoLyricsOverlay/3.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.h) return false;
    WinHttpSetTimeouts(session.h, 5000, 5000, 10000, 15000);
    WH conn(WinHttpConnect(session.h, host.c_str(), uc.nPort, 0));
    if (!conn.h) return false;
    WH req(WinHttpOpenRequest(conn.h, method, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                              WINHTTP_DEFAULT_ACCEPT_TYPES, plainOk ? 0 : WINHTTP_FLAG_SECURE));
    if (!req.h) return false;
    if (!allowRedirect) {
        DWORD o = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &o, sizeof(o));
    }

    std::wstring hdrs = L"Accept: application/json\r\n";
    if (host == L"music.163.com") hdrs += L"Referer: https://music.163.com/\r\nCookie: appver=2.0.2\r\n";
    if (zing) {
        hdrs += L"Referer: https://zingmp3.vn/\r\n";
        std::lock_guard<std::mutex> lk(g_zingMtx);
        if (!g_zingCookie.empty()) hdrs += L"Cookie: " + Widen(g_zingCookie) + L"\r\n";
    }
    hdrs += extraHdrs;
    if (!body.empty()) hdrs += std::wstring(L"Content-Type: ") + contentType + L"\r\n";
    BOOL ok = WinHttpSendRequest(req.h, hdrs.c_str(), (DWORD)-1L,
                                 body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                 (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!ok || !WinHttpReceiveResponse(req.h, nullptr)) return false;

    if (zing && path == L"/") {  // trang chủ: lấy cookie phiên cho các lần gọi API
        std::string cookies;
        DWORD idx = 0;
        for (;;) {
            wchar_t cb[1024];
            DWORD csz = sizeof(cb);
            if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX, cb, &csz,
                                     &idx))
                break;
            std::wstring c(cb);
            c = c.substr(0, c.find(L';'));
            if (c.empty()) continue;
            if (!cookies.empty()) cookies += "; ";
            for (wchar_t ch : c) cookies += (char)(ch < 128 ? ch : '?');
        }
        if (!cookies.empty()) {
            std::lock_guard<std::mutex> lk(g_zingMtx);
            g_zingCookie = cookies;
        }
    }

    DWORD status = 0, sz = sizeof(status);
    if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX))
        return false;
    if (status != 200) return false;

    if (wantImage) {
        wchar_t ct[128] = {};
        DWORD cs = sizeof(ct);
        if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, ct,
                                 &cs, WINHTTP_NO_HEADER_INDEX))
            return false;
        if (_wcsnicmp(ct, L"image/", 6) != 0) return false;
    }

    out.clear();
    while (true) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail)) return false;
        if (avail == 0) break;
        size_t old = out.size();
        if (old + avail > maxBytes) return false;
        out.resize(old + avail);
        DWORD got = 0;
        if (!WinHttpReadData(req.h, &out[old], avail, &got)) return false;
        out.resize(old + got);
        if (!g_running) return false;
    }
    return true;
}

static bool HttpGetJson(const std::string& url, JV& out) {
    std::string body;
    if (!HttpFetch(L"GET", url, "", body, 4 * 1024 * 1024, true, false)) return false;
    return ParseJson(body, out);
}

// ---------------------------------------------------------------------------
// Lyrics providers
// ---------------------------------------------------------------------------

static std::string Norm(const std::string& v) {
    std::string s = Lower(v), t;
    int d = 0;
    for (char c : s) {
        if (c == '(' || c == '[') d++;
        else if (c == ')' || c == ']') { if (d > 0) d--; }
        else if (d == 0) t += c;
    }
    std::string o;
    bool sp = false;
    for (unsigned char c : t) {
        if (isalnum(c) || c >= 0x80) {
            if (sp && !o.empty()) o += ' ';
            sp = false;
            o += (char)c;
        } else {
            sp = true;
        }
    }
    return o;
}

static double Similar(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0;
    size_t n = a.size(), m = b.size();
    std::vector<size_t> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; j++) prev[j] = j;
    for (size_t i = 1; i <= n; i++) {
        cur[0] = i;
        for (size_t j = 1; j <= m; j++) {
            size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, cur);
    }
    return 1.0 - (double)prev[m] / (double)std::max(n, m);
}

static bool ExtractTimed(const JV& arr, Lines& lines, bool msHeuristic) {
    if (arr.t != JV::Arr) return false;
    for (const JV& it : arr.a) {
        if (it.t != JV::Obj) continue;
        std::string text = Trim(it.str("text"));
        if (text.empty()) text = Trim(it.str("words"));
        double stamp = it.num("start_time", it.num("time", -1));
        if (text.empty() || stamp < 0) continue;
        if (msHeuristic) {
            if (stamp > 10000) stamp /= 1000.0;
        } else {
            stamp /= 1000.0;
        }
        lines.emplace_back(stamp, text);
    }
    std::stable_sort(lines.begin(), lines.end(),
                     [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
                         return a.first < b.first;
                     });
    return !lines.empty();
}

static bool Contains(const std::string& h, const std::string& n) {
    return n.size() >= 2 && h.find(n) != std::string::npos;
}

static bool IsCreditLine(const std::string& t) {
    static const char* const prefixes[] = {"作词", "作詞", "作曲", "编曲", "編曲", "制作", "製作", "混音",
                                           "母带", "录音", "监制", "Lyrics by", "Composer", "Composed by",
                                           "Written by", "Arranged by", "Produced by"};
    bool colon = t.find(':') != std::string::npos || t.find("：") != std::string::npos;
    if (!colon) return false;
    for (const char* pf : prefixes)
        if (t.compare(0, strlen(pf), pf) == 0) return true;
    return false;
}

// NetEase Cloud Music (API không chính thức): tìm bài rồi lấy lời LRC.
static bool FindNetease(const std::string& title, const std::string& artist, LyricsResult& res) {
    JV p;
    std::string url = "https://music.163.com/api/search/get?s=" + UrlEnc(Trim(title + " " + artist)) +
                      "&type=1&limit=10&offset=0";
    if (!HttpGetJson(url, p) || p.t != JV::Obj) return false;
    const JV* r = p.get("result");
    if (!r || r->t != JV::Obj) return false;
    const JV* songs = r->get("songs");
    if (!songs || songs->t != JV::Arr) return false;

    struct Cand {
        double score;
        double id;
        std::string name, art;
    };
    std::vector<Cand> cand;
    std::string tt = Norm(title), ta = Norm(artist);

    for (const JV& sg : songs->a) {
        if (sg.t != JV::Obj) continue;
        double id = sg.num("id", 0);
        if (id <= 0) continue;
        std::string name = sg.str("name"), art;
        const JV* ars = sg.get("artists");
        if (ars && ars->t == JV::Arr) {
            for (const JV& a : ars->a) {
                std::string n = a.str("name");
                if (n.empty()) continue;
                if (!art.empty()) art += ", ";
                art += n;
            }
        }
        std::string cn = Norm(name), ca = Norm(art);
        double ts = Similar(tt, cn);
        if (Contains(tt, cn) || Contains(cn, tt)) ts = std::max(ts, 0.85);
        if (ts < 0.65) continue;
        double as = 0;
        if (!ta.empty() && !ca.empty()) {
            as = Similar(ta, ca);
            if (Contains(ta, ca) || Contains(ca, ta) || Contains(tt, ca)) as = std::max(as, 0.85);
        }
        cand.push_back({ts * 2 + as, id, name, art});
    }
    std::stable_sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });

    int tries = 0;
    for (const Cand& c : cand) {
        if (!g_running || tries++ >= 3) break;
        char idb[32];
        snprintf(idb, sizeof idb, "%.0f", c.id);
        JV l;
        if (!HttpGetJson(std::string("https://music.163.com/api/song/lyric?id=") + idb + "&lv=1&kv=1&tv=-1", l) ||
            l.t != JV::Obj)
            continue;
        const JV* lrc = l.get("lrc");
        if (!lrc || lrc->t != JV::Obj) continue;

        Lines raw = ParseLrc(lrc->str("lyric")), lines;
        for (auto& ln : raw)
            if (!IsCreditLine(ln.second)) lines.push_back(ln);
        if (lines.size() < 3) continue;  // nhạc không lời / chỉ có thông tin

        res.lines = lines;
        res.status = (c.name.empty() ? title : c.name) + " — " + (c.art.empty() ? artist : c.art);
        return true;
    }
    return false;
}

// ---- Zing MP3 (API không chính thức của web, cần ký yêu cầu) ----
// Cách ký theo https://github.com/whoant/ZingMp3API (modules/ZingMp3.js):
//   sig = HMAC-SHA512(SECRET, path + SHA256("ctime=<t>" [+ "id=<id>"]))
//   query = <tham số> & ctime & sig & apiKey      (không có "version")
// Tìm kiếm (haveParam=1) chỉ hash riêng ctime; "q" không nằm trong chữ ký.
// Nếu Zing yêu cầu version, đặt ZING_VER = "1.6.34" (sẽ được đưa vào cả hash lẫn query).

static const char* ZING_KEY = "88265e23d4284f25963e6eedac8fbfa3";
static const char* ZING_SECRET = "2aa2d1c561e809b267f3638c4a307aab";
static const char* ZING_VER = "";

static std::string VerHash() {
    return *ZING_VER ? std::string("version=") + ZING_VER : std::string();
}
static std::string VerQuery() {
    return *ZING_VER ? std::string("&version=") + ZING_VER : std::string();
}

static std::string HexStr(const BYTE* d, size_t n) {
    static const char* hx = "0123456789abcdef";
    std::string o;
    for (size_t i = 0; i < n; i++) {
        o += hx[d[i] >> 4];
        o += hx[d[i] & 15];
    }
    return o;
}

static std::string Sha256Hex(const std::string& in) {
    BCRYPT_ALG_HANDLE h = nullptr;
    if (BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return "";
    BYTE out[32];
    NTSTATUS st = BCryptHash(h, nullptr, 0, (PUCHAR)in.data(), (ULONG)in.size(), out, sizeof(out));
    BCryptCloseAlgorithmProvider(h, 0);
    return st == 0 ? HexStr(out, sizeof(out)) : "";
}

static std::string HmacSha512Hex(const std::string& key, const std::string& msg) {
    BCRYPT_ALG_HANDLE h = nullptr;
    if (BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA512_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return "";
    BYTE out[64];
    NTSTATUS st = BCryptHash(h, (PUCHAR)key.data(), (ULONG)key.size(), (PUCHAR)msg.data(), (ULONG)msg.size(),
                             out, sizeof(out));
    BCryptCloseAlgorithmProvider(h, 0);
    return st == 0 ? HexStr(out, sizeof(out)) : "";
}

static std::string ZingSig(const std::string& path, const std::string& joined) {
    std::string h = Sha256Hex(joined);
    return h.empty() ? "" : HmacSha512Hex(ZING_SECRET, path + h);
}

static void ZingClearCookie() {
    std::lock_guard<std::mutex> lk(g_zingMtx);
    g_zingCookie.clear();
}

static bool ZingEnsureCookie() {
    {
        std::lock_guard<std::mutex> lk(g_zingMtx);
        if (!g_zingCookie.empty()) return true;
    }
    std::string page;
    HttpFetch(L"GET", "https://zingmp3.vn/", "", page, 8 * 1024 * 1024, true, false);
    std::lock_guard<std::mutex> lk(g_zingMtx);
    return !g_zingCookie.empty();
}

static std::string StripAngleTags(const std::string& t) {
    std::string o;
    bool in = false;
    for (char c : t) {
        if (c == '<') in = true;
        else if (c == '>' && in) in = false;
        else if (!in) o += c;
    }
    return Trim(o);
}

static bool EndsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

static bool FindZing(const std::string& title, const std::string& artist, LyricsResult& res) {
    if (!ZingEnsureCookie()) return false;
    std::string ct = std::to_string((long long)time(nullptr));

    // 1. Tìm bài (kiểu whoant: hash chỉ gồm ctime)
    std::string path = "/api/v2/search/multi";
    std::string sig = ZingSig(path, "ctime=" + ct + VerHash());
    if (sig.empty()) return false;
    std::string url = "https://zingmp3.vn" + path + "?q=" + UrlEnc(Trim(title + " " + artist)) +
                      "&ctime=" + ct + VerQuery() +
                      "&sig=" + sig + "&apiKey=" + ZING_KEY;
    JV p;
    if (!HttpGetJson(url, p) || p.t != JV::Obj || p.num("err", -1) != 0) {
        ZingClearCookie();  // cookie/chữ ký có thể hết hạn: lấy lại ở lần sau
        return false;
    }
    const JV* d = p.get("data");
    if (!d || d->t != JV::Obj) return false;
    const JV* items = d->get("songs");
    if (!items || items->t != JV::Arr) items = d->get("items");
    if (!items || items->t != JV::Arr) return false;

    struct Cand {
        double score;
        std::string id, name, art;
    };
    std::vector<Cand> cand;
    std::string tt = Norm(title), ta = Norm(artist);
    for (const JV& it : items->a) {
        if (it.t != JV::Obj) continue;
        std::string id = it.str("encodeId");
        if (id.empty()) continue;
        std::string name = it.str("title"), art = it.str("artistsNames");
        std::string cn = Norm(name), ca = Norm(art);
        double ts = Similar(tt, cn);
        if (Contains(tt, cn) || Contains(cn, tt)) ts = std::max(ts, 0.85);
        if (ts < 0.65) continue;
        double as = 0;
        if (!ta.empty() && !ca.empty()) {
            as = Similar(ta, ca);
            if (Contains(ta, ca) || Contains(ca, ta) || Contains(tt, ca)) as = std::max(as, 0.85);
        }
        cand.push_back({ts * 2 + as, id, name, art});
    }
    std::stable_sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });

    // 2. Lấy lời (hash gồm ctime + id)
    int tries = 0;
    for (const Cand& c : cand) {
        if (!g_running || tries++ >= 3) break;
        std::string lpath = "/api/v2/lyric/get/lyric";
        std::string ls = ZingSig(lpath, "ctime=" + ct + "id=" + c.id + VerHash());
        if (ls.empty()) return false;
        std::string lurl = "https://zingmp3.vn" + lpath + "?id=" + UrlEnc(c.id) +
                           "&ctime=" + ct + VerQuery() + "&sig=" + ls +
                           "&apiKey=" + ZING_KEY;
        JV l;
        if (!HttpGetJson(lurl, l) || l.t != JV::Obj || l.num("err", -1) != 0) continue;
        const JV* ld = l.get("data");
        if (!ld || ld->t != JV::Obj) continue;

        Lines lines;

        // Ưu tiên file LRC (chỉ từ CDN của Zing)
        std::string file = ld->str("file"), host, body;
        if (!file.empty() && HttpsHost(file, host) && EndsWith(host, "zmdcdn.me") &&
            HttpFetch(L"GET", file, "", body, 2 * 1024 * 1024, false, false)) {
            for (auto& ln : ParseLrc(body)) {
                std::string t = StripAngleTags(ln.second);
                if (!t.empty()) lines.emplace_back(ln.first, t);
            }
        }

        // Dự phòng: ghép từ từng từ có mốc thời gian
        if (lines.size() < 3) {
            lines.clear();
            const JV* sents = ld->get("sentences");
            if (sents && sents->t == JV::Arr) {
                for (const JV& sn : sents->a) {
                    const JV* words = sn.get("words");
                    if (!words || words->t != JV::Arr || words->a.empty()) continue;
                    std::string text;
                    for (const JV& w : words->a) {
                        std::string tk = Trim(w.str("data"));
                        if (tk.empty()) continue;
                        if (!text.empty()) text += ' ';
                        text += tk;
                    }
                    double st = words->a[0].num("startTime", -1);
                    if (!text.empty() && st >= 0) lines.emplace_back(st / 1000.0, text);
                }
                std::stable_sort(lines.begin(), lines.end(),
                                 [](const std::pair<double, std::string>& a,
                                    const std::pair<double, std::string>& b) { return a.first < b.first; });
            }
        }

        if (lines.size() < 3) continue;
        res.lines = lines;
        res.status = (c.name.empty() ? title : c.name) + " — " + (c.art.empty() ? artist : c.art);
        return true;
    }
    return false;
}

// ---- NhacCuaTui (API di động không chính thức, theo github.com/tuannvbg/nct-api-v2) ----
// Luồng: tìm bài (v5 search-song) -> token v2 -> chi tiết bài (v2) lấy số ID từ link stream
//        -> lời (v5 get-lyric). Tên trường JSON của kết quả tìm kiếm là phỏng đoán,
//        nếu không đọc được mod ghi log "NCT search:" / "NCT lyric:" để chỉnh lại.

static const char* NCT_V1_KEY = "nct@asdgvhfhyth1391515932000";
static std::mutex g_nctMtx;
static std::string g_nctToken;
static ULONGLONG g_nctExpire = 0, g_nctRetryAt = 0;

static std::string Md5Hex(const std::string& in) {
    BCRYPT_ALG_HANDLE h = nullptr;
    if (BCryptOpenAlgorithmProvider(&h, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0) return "";
    BYTE out[16];
    NTSTATUS st = BCryptHash(h, nullptr, 0, (PUCHAR)in.data(), (ULONG)in.size(), out, sizeof(out));
    BCryptCloseAlgorithmProvider(h, 0);
    return st == 0 ? HexStr(out, sizeof(out)) : "";
}

// Dữ liệu v2 của NCT là mảng đánh chỉ số: chấp nhận cả dạng mảng lẫn object khóa "7".
static const JV* NctIdx(const JV* d, int i) {
    if (!d) return nullptr;
    if (d->t == JV::Arr) return (size_t)i < d->a.size() ? &d->a[i] : nullptr;
    if (d->t == JV::Obj) return d->get(std::to_string(i).c_str());
    return nullptr;
}

static bool NctResultOk(const JV& o) {
    const JV* r = o.get("Result");
    return r && ((r->t == JV::Num && r->n == 1) || (r->t == JV::Bool && r->b));
}

static bool NctToken(std::string& tok) {
    std::lock_guard<std::mutex> lk(g_nctMtx);
    ULONGLONG now = GetTickCount64();
    if (!g_nctToken.empty() && now < g_nctExpire) { tok = g_nctToken; return true; }
    if (now < g_nctRetryAt) return false;

    static const char* form =
        "deviceinfo=%7B%22DeviceID%22%3A%22dd03852ada21ec149103d02f76eb0a04%22%2C%22DeviceName%22%3A%22TrolyFaceBook.Com%22%2C%22OsName%22%3A%22SmartTV%22%2C%22OsVersion%22%3A%228.0%22%2C%22AppName%22%3A%22NCTTablet%22%2C%22AppVersion%22%3A%221.3.0%22%2C%22UserName%22%3A%220%22%2C%22QualityPlay%22%3A%22128%22%2C%22QualityDownload%22%3A%22128%22%2C%22QualityCloud%22%3A%22128%22%2C%22Network%22%3A%22WIFI%22%2C%22Provider%22%3A%22BeDieuApp%22%7D%0A"
        "&md5=488c994e95caa50344d217e9926caf76&timestamp=1497863709521";
    std::string body;
    JV j;
    bool ok = HttpFetch(L"POST", "https://graph.nhaccuatui.com/v2/commons/token", form, body, 64 * 1024, true,
                        false, L"", L"application/x-www-form-urlencoded") &&
              ParseJson(body, j) && j.t == JV::Obj;
    const JV* d = ok ? j.get("data") : nullptr;
    std::string t = (d && d->t == JV::Obj) ? d->str("accessToken") : std::string();
    if (t.empty()) { g_nctRetryAt = now + 60000; return false; }  // lỗi: nghỉ 60s
    g_nctToken = t;
    g_nctExpire = now + 20ULL * 60 * 1000;
    tok = t;
    return true;
}

static std::string NctV1Url(const char* action, const std::string& token, int size, const std::string& opt) {
    static const char* devinfo =
        "{\"DeviceID\":\"90c18c4cb3c37d442e8386631d46b46f\",\"OsName\":\"ANDROID\",\"OsVersion\":\"10\","
        "\"AppName\":\"NhacCuaTui\",\"AppVersion\":\"5.0.1\",\"UserInfo\":\"\",\"LocationInfo\":\"\"}";
    return std::string("http://api.m.nhaccuatui.com/mobile/v5.0/api?secretkey=nct@mobile_service&deviceinfo=") +
           UrlEnc(devinfo) + "&pageindex=1&pagesize=" + std::to_string(size) +
           "&time=1391515932000&token=" + token + "&action=" + action + "&" + opt;
}

static bool FindNct(const std::string& title, const std::string& artist, LyricsResult& res) {
    std::string kw = Trim(title + " " + artist);
    std::string stok = Md5Hex("search-song" + kw + "1" + "10" + NCT_V1_KEY);
    if (stok.empty()) return false;

    std::string raw;
    JV p;
    if (!HttpFetch(L"GET", NctV1Url("search-song", stok, 10, "keyword=" + UrlEnc(kw)), "", raw, 1024 * 1024,
                   true, false) ||
        !ParseJson(raw, p) || p.t != JV::Obj || !NctResultOk(p))
        return false;

    const JV* data = p.get("Data");
    const JV* arr = nullptr;
    if (data && data->t == JV::Arr) arr = data;
    else if (data && data->t == JV::Obj)
        for (const JV& v : data->vals)
            if (v.t == JV::Arr) { arr = &v; break; }
    if (!arr) {
        Wh_Log(L"NCT search: không đọc được Data: %s", Widen(Trunc(raw, 300)).c_str());
        return false;
    }

    auto pick = [](const JV& o, std::initializer_list<const char*> ks) {
        for (const char* k : ks) {
            std::string s = o.str(k);
            if (!s.empty()) return s;
        }
        return std::string();
    };

    struct Cand {
        double score;
        std::string key, name, art;
    };
    std::vector<Cand> cand;
    std::string tt = Norm(title), ta = Norm(artist);
    for (const JV& it : arr->a) {
        if (it.t != JV::Obj) continue;
        std::string key = pick(it, {"Key", "SongKey", "key", "Id"});
        std::string name = pick(it, {"Title", "SongTitle", "Name"});
        std::string art = pick(it, {"ArtistName", "Artist", "SingerTitle", "Singer"});
        if (key.empty() || name.empty()) continue;
        std::string cn = Norm(name), ca = Norm(art);
        double ts = Similar(tt, cn);
        if (Contains(tt, cn) || Contains(cn, tt)) ts = std::max(ts, 0.85);
        if (ts < 0.65) continue;
        double as = 0;
        if (!ta.empty() && !ca.empty()) {
            as = Similar(ta, ca);
            if (Contains(ta, ca) || Contains(ca, ta) || Contains(tt, ca)) as = std::max(as, 0.85);
        }
        cand.push_back({ts * 2 + as, key, name, art});
    }
    std::stable_sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });

    int tries = 0;
    for (const Cand& c : cand) {
        if (!g_running || tries++ >= 3) break;

        // 1. Chi tiết bài (v2) -> số ID nằm cuối tên file stream
        std::string at;
        if (!NctToken(at)) return false;
        JV d;
        if (!HttpGetJson("https://graph.nhaccuatui.com/v2/songs/" + UrlEnc(c.key) + "?access_token=" + UrlEnc(at),
                         d) ||
            d.t != JV::Obj)
            continue;
        const JV* sNode = NctIdx(d.get("data"), 7);
        std::string stream = (sNode && sNode->t == JV::Str) ? sNode->s : std::string();
        size_t q = stream.find_first_of("?#");
        if (q != std::string::npos) stream.resize(q);
        size_t sl = stream.rfind('/');
        std::string fn = sl == std::string::npos ? stream : stream.substr(sl + 1);
        size_t dash = fn.rfind('-');
        if (dash == std::string::npos) continue;
        std::string id = fn.substr(dash + 1);
        id = id.substr(0, id.find('.'));
        if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos) continue;

        // 2. Lấy lời (v5)
        std::string ltok = Md5Hex("get-lyric" + id + NCT_V1_KEY);
        std::string lraw;
        JV l;
        if (ltok.empty() ||
            !HttpFetch(L"GET", NctV1Url("get-lyric", ltok, 30, "songid=" + id), "", lraw, 1024 * 1024, true,
                       false) ||
            !ParseJson(lraw, l) || l.t != JV::Obj || !NctResultOk(l))
            continue;
        const JV* ld = l.get("Data");
        if (!ld || ld->t != JV::Obj) continue;

        std::string timed = Trim(ld->str("TimedLyric")), text;
        if (timed.compare(0, 4, "http") == 0) {  // link file .lrc: chỉ nhận từ CDN của NCT
            if (timed.compare(0, 7, "http://") == 0) timed = "https://" + timed.substr(7);
            std::string host;
            if (!HttpsHost(timed, host) || !(EndsWithStr(host, ".nixcdn.com") || EndsWithStr(host, ".nhaccuatui.com")))
                continue;
            if (!HttpFetch(L"GET", timed, "", text, 2 * 1024 * 1024, true, false)) continue;
        } else {
            text = timed;
        }

        Lines lines;
        for (auto& ln : ParseLrc(text))
            if (!IsCreditLine(ln.second)) lines.push_back(ln);
        if (lines.size() < 3) {
            Wh_Log(L"NCT lyric: không có LRC dùng được: %s", Widen(Trunc(lraw, 200)).c_str());
            continue;
        }
        res.lines = lines;
        res.status = c.name + " — " + (c.art.empty() ? artist : c.art);
        return true;
    }
    return false;
}

// ---- YouTube Music lyrics (innertube, không chính thức) ----
// Luồng: next(videoId) -> browseId của tab Lyrics (MPLYt_...) -> browse (client ANDROID_MUSIC)
// -> timedLyricsData (lời có mốc thời gian). Tìm khóa theo kiểu đệ quy nên ít vỡ khi cấu trúc đổi.

static bool IsVideoIdCh(char c) { return isalnum((unsigned char)c) || c == '-' || c == '_'; }

static std::string ExtractVideoId(const std::string& url) {
    size_t q = url.find('?');
    if (q == std::string::npos) return "";
    size_t p = q;
    while ((p = url.find("v=", p)) != std::string::npos) {
        if (url[p - 1] == '?' || url[p - 1] == '&') {
            std::string id = url.substr(p + 2, 11);
            if (id.size() != 11) return "";
            for (char c : id)
                if (!IsVideoIdCh(c)) return "";
            return id;
        }
        p += 2;
    }
    return "";
}

static const JV* YtmFindKey(const JV& v, const char* key, int depth = 0) {
    if (depth > 100) return nullptr;
    if (v.t == JV::Obj) {
        for (size_t i = 0; i < v.keys.size(); i++)
            if (v.keys[i] == key) return &v.vals[i];
        for (const JV& c : v.vals)
            if (const JV* r = YtmFindKey(c, key, depth + 1)) return r;
    } else if (v.t == JV::Arr) {
        for (const JV& c : v.a)
            if (const JV* r = YtmFindKey(c, key, depth + 1)) return r;
    }
    return nullptr;
}

static bool YtmFindLyricsBrowseId(const JV& v, std::string& out, int depth = 0) {
    if (depth > 100) return false;
    if (v.t == JV::Obj) {
        for (size_t i = 0; i < v.keys.size(); i++) {
            if (v.keys[i] == "browseId" && v.vals[i].t == JV::Str && v.vals[i].s.compare(0, 5, "MPLYt") == 0) {
                out = v.vals[i].s;
                return true;
            }
        }
        for (const JV& c : v.vals)
            if (YtmFindLyricsBrowseId(c, out, depth + 1)) return true;
    } else if (v.t == JV::Arr) {
        for (const JV& c : v.a)
            if (YtmFindLyricsBrowseId(c, out, depth + 1)) return true;
    }
    return false;
}

static bool YtmPost(const char* endpoint, const std::string& body, JV& out) {
    std::string raw;
    std::string url = std::string("https://music.youtube.com/youtubei/v1/") + endpoint + "?prettyPrint=false";
    if (!HttpFetch(L"POST", url, body, raw, 8 * 1024 * 1024, true, false, L"Origin: https://music.youtube.com\r\n"))
        return false;
    return ParseJson(raw, out) && out.t == JV::Obj;
}

static bool FindYtMusic(const std::string& videoId, const std::string& title, const std::string& artist,
                        LyricsResult& res) {
    if (videoId.size() != 11) return false;

    // 1. Lấy browseId của tab Lyrics
    JV nx;
    std::string b1 = "{\"context\":{\"client\":{\"clientName\":\"WEB_REMIX\",\"clientVersion\":\"1.20250310.01.00\","
                     "\"hl\":\"en\"}},\"videoId\":\"" + videoId + "\",\"isAudioOnly\":true}";
    if (!YtmPost("next", b1, nx)) return false;
    std::string browseId;
    if (!YtmFindLyricsBrowseId(nx, browseId)) {
        Wh_Log(L"YTM: bài này không có tab lời");
        return false;
    }

    // 2. Lấy lời có mốc thời gian (cần client ANDROID_MUSIC)
    JV br;
    std::string b2 = "{\"context\":{\"client\":{\"clientName\":\"ANDROID_MUSIC\",\"clientVersion\":\"7.21.50\","
                     "\"hl\":\"en\"}},\"browseId\":\"" + browseId + "\"}";
    if (!YtmPost("browse", b2, br)) return false;
    const JV* arr = YtmFindKey(br, "timedLyricsData");
    if (!arr || arr->t != JV::Arr) {
        Wh_Log(L"YTM: không có timedLyricsData (chỉ có lời không đồng bộ?)");
        return false;
    }

    Lines lines;
    for (const JV& it : arr->a) {
        if (it.t != JV::Obj) continue;
        std::string text = Trim(it.str("lyricLine"));
        const JV* cue = it.get("cueRange");
        if (!cue || cue->t != JV::Obj) continue;
        const JV* st = cue->get("startTimeMilliseconds");
        double ms = -1;
        if (st && st->t == JV::Str) ms = strtod(st->s.c_str(), nullptr);
        else if (st && st->t == JV::Num) ms = st->n;
        if (ms < 0) continue;
        lines.emplace_back(ms / 1000.0, text);
    }
    std::stable_sort(lines.begin(), lines.end(),
                     [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
                         return a.first < b.first;
                     });
    if (lines.size() < 3) return false;

    res.lines = lines;
    res.status = title + " — " + (artist.empty() ? std::string("YouTube Music") : artist);
    return true;
}

// ---- Musixmatch (endpoint desktop không chính thức, token ẩn danh) ----
// Luồng: token.get -> user_token -> macro.subtitles.get (subtitle_format=lrc) -> subtitle_body (LRC).
// Nếu Musixmatch bắt captcha / từ chối token thì token trả về dạng "UpgradeOnly..." hoặc status 401:
// mod bỏ token, nghỉ 60s rồi xin lại. Tên bài trả về được so khớp lại để tránh nhận nhầm bài.

static const char* MXM_ROOT = "https://apic-desktop.musixmatch.com/ws/1.1/";
static const wchar_t* MXM_HDRS = L"Cookie: AWSELBCORS=0; AWSELB=0\r\n";
static std::mutex g_mxmMtx;
static std::string g_mxmToken;
static ULONGLONG g_mxmExpire = 0, g_mxmRetryAt = 0;

// Đi theo chuỗi khóa object: JPath(&root, {"message","body"}).
static const JV* JPath(const JV* v, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        if (!v || v->t != JV::Obj) return nullptr;
        v = v->get(k);
    }
    return v;
}

static bool MxmToken(std::string& tok, bool force) {
    std::lock_guard<std::mutex> lk(g_mxmMtx);
    ULONGLONG now = GetTickCount64();
    if (force) { g_mxmToken.clear(); g_mxmExpire = 0; }
    if (!g_mxmToken.empty() && now < g_mxmExpire) { tok = g_mxmToken; return true; }
    if (now < g_mxmRetryAt) return false;

    std::string raw;
    JV j;
    std::string url = std::string(MXM_ROOT) +
                      "token.get?format=json&user_language=en&app_id=web-desktop-app-v1.0&t=" +
                      std::to_string((unsigned long long)now);
    bool ok = HttpFetch(L"GET", url, "", raw, 256 * 1024, true, false, MXM_HDRS) && ParseJson(raw, j) &&
              j.t == JV::Obj;
    const JV* body = ok ? JPath(&j, {"message", "body"}) : nullptr;
    std::string t = (body && body->t == JV::Obj) ? body->str("user_token") : std::string();
    if (t.empty() || t.compare(0, 11, "UpgradeOnly") == 0) {
        Wh_Log(L"MXM: không lấy được token (captcha/giới hạn?), nghỉ 60s");
        g_mxmRetryAt = now + 60000;
        return false;
    }
    g_mxmToken = t;
    g_mxmExpire = now + 10ULL * 60 * 1000;
    tok = t;
    return true;
}

static bool FindMusixmatch(const std::string& title, const std::string& artist, LyricsResult& res) {
    for (int attempt = 0; attempt < 2 && g_running; attempt++) {
        std::string tok;
        if (!MxmToken(tok, attempt == 1)) return false;

        std::string url = std::string(MXM_ROOT) +
                          "macro.subtitles.get?format=json&namespace=lyrics_richsynched&subtitle_format=lrc"
                          "&app_id=web-desktop-app-v1.0&user_language=en&q_track=" + UrlEnc(title) +
                          "&q_artist=" + UrlEnc(artist) + "&q_artists=" + UrlEnc(artist) +
                          "&usertoken=" + UrlEnc(tok);
        std::string raw;
        JV j;
        if (!HttpFetch(L"GET", url, "", raw, 4 * 1024 * 1024, true, false, MXM_HDRS) || !ParseJson(raw, j) ||
            j.t != JV::Obj)
            return false;

        const JV* hdr = JPath(&j, {"message", "header"});
        double code = (hdr && hdr->t == JV::Obj) ? hdr->num("status_code", 200) : 200;
        if (code == 401 || code == 402) {  // token bị từ chối: xin token mới rồi thử lại một lần
            Wh_Log(L"MXM: token bị từ chối (%d)", (int)code);
            continue;
        }

        const JV* mc = JPath(&j, {"message", "body", "macro_calls"});
        if (!mc || mc->t != JV::Obj) {
            Wh_Log(L"MXM: phản hồi không có macro_calls: %s", Widen(Trunc(raw, 200)).c_str());
            return false;
        }

        // Kiểm tra bài Musixmatch khớp có đúng bài đang phát không.
        const JV* tr = JPath(mc, {"matcher.track.get", "message", "body", "track"});
        if (!tr || tr->t != JV::Obj) {
            Wh_Log(L"MXM: không khớp được bài");
            return false;
        }
        std::string name = tr->str("track_name"), art = tr->str("artist_name");
        std::string tt = Norm(title), ta = Norm(artist), cn = Norm(name), ca = Norm(art);
        double ts = Similar(tt, cn);
        if (Contains(tt, cn) || Contains(cn, tt)) ts = std::max(ts, 0.85);
        if (ts < 0.65) {
            Wh_Log(L"MXM: bài khớp \"%s\" không giống \"%s\"", Widen(name).c_str(), Widen(title).c_str());
            return false;
        }
        if (!ta.empty() && !ca.empty()) {
            double as = Similar(ta, ca);
            bool artistOk = as >= 0.4 || Contains(ta, ca) || Contains(ca, ta) || Contains(tt, ca);
            if (!artistOk) {
                Wh_Log(L"MXM: ca sĩ khớp \"%s\" không giống \"%s\"", Widen(art).c_str(), Widen(artist).c_str());
                return false;
            }
        }

        const JV* list = JPath(mc, {"track.subtitles.get", "message", "body", "subtitle_list"});
        if (!list || list->t != JV::Arr || list->a.empty()) return false;
        const JV* sb = JPath(&list->a[0], {"subtitle", "subtitle_body"});
        if (!sb || sb->t != JV::Str) return false;

        Lines lines;
        for (auto& ln : ParseLrc(sb->s))
            if (!IsCreditLine(ln.second)) lines.push_back(ln);
        if (lines.size() < 3) return false;

        res.lines = lines;
        res.status = (name.empty() ? title : name) + " — " + (art.empty() ? artist : art);
        return true;
    }
    return false;
}

// ---- Làm sạch tiêu đề / tên kênh YouTube để tìm lời chính xác hơn ----
// Tiêu đề YouTube thường là "Ca sĩ - Tên bài (Official MV) | ..." còn "ca sĩ" là tên kênh
// ("xxxVEVO", "Artist - Topic"...). Các nguồn lời cần tên bài + ca sĩ sạch mới tìm ra.

static bool StartsAt(const std::string& s, size_t i, const char* p) {
    return s.compare(i, strlen(p), p) == 0;
}

static bool IsWordCh(unsigned char c) { return isalnum(c) || c >= 0x80; }

static std::string ReplaceAll(std::string s, const char* from, const char* to) {
    size_t n = strlen(from), m = strlen(to), p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, n, to);
        p += m;
    }
    return s;
}

// Xóa nội dung trong (), [], {}, <>, 【】, 「」, 『』, （）.
static std::string EraseBrackets(const std::string& s) {
    static const char* const opens[] = {"(", "[", "{", "<", "【", "「", "『", "（"};
    static const char* const closes[] = {")", "]", "}", ">", "】", "」", "』", "）"};
    std::string o;
    int d = 0;
    for (size_t i = 0; i < s.size();) {
        bool hit = false;
        for (const char* p : opens)
            if (StartsAt(s, i, p)) { d++; i += strlen(p); hit = true; break; }
        if (hit) continue;
        for (const char* p : closes)
            if (StartsAt(s, i, p)) { if (d > 0) d--; i += strlen(p); hit = true; break; }
        if (hit) continue;
        if (d == 0) o += s[i];
        i++;
    }
    return o;
}

// Xóa một cụm từ (không phân biệt hoa/thường với ASCII, chỉ khi đứng riêng như một từ).
static std::string RemovePhrase(const std::string& s, const char* phrase) {
    std::string low = Lower(s), o;
    size_t n = strlen(phrase), i = 0;
    while (i < s.size()) {
        if (low.compare(i, n, phrase) == 0 && (i == 0 || !IsWordCh((unsigned char)s[i - 1])) &&
            (i + n >= s.size() || !IsWordCh((unsigned char)s[i + n]))) {
            i += n;
            continue;
        }
        o += s[i++];
    }
    return o;
}

static std::string RemoveNoise(std::string s) {
    static const char* const noise[] = {
        "official music video", "official lyric video", "official lyrics video", "official video",
        "official audio", "official mv", "official visualizer", "music video", "lyric video",
        "lyrics video", "video lyrics", "lyrics", "lyric", "visualizer", "official", "audio",
        "m/v", "mv", "hd", "hq", "4k", "1080p", "karaoke"};
    for (const char* p : noise) s = RemovePhrase(s, p);
    return s;
}

// Gộp khoảng trắng, bỏ ký tự phân tách thừa ở hai đầu.
static std::string TidyText(const std::string& in) {
    std::string o;
    bool sp = false;
    for (char c : in) {
        if ((unsigned char)c <= ' ') { sp = true; continue; }
        if (sp && !o.empty()) o += ' ';
        sp = false;
        o += c;
    }
    auto isSep = [](char c) {
        return c == '-' || c == '|' || c == ':' || c == ',' || c == '~' || c == '/' || c == '.' ||
               c == ' ' || c == '"' || c == '\'';
    };
    size_t b = 0, e = o.size();
    while (b < e && isSep(o[b])) b++;
    while (e > b && isSep(o[e - 1])) e--;
    return o.substr(b, e - b);
}

// Cắt phần "ft. ..." / "feat. ..." (ca sĩ khách mời) khỏi một đoạn.
static std::string CutFeat(const std::string& s) {
    static const char* const marks[] = {" ft.", " ft ", " feat.", " feat ", " featuring "};
    std::string low = Lower(s);
    size_t cut = std::string::npos;
    for (const char* m : marks) {
        size_t p = low.find(m);
        if (p != std::string::npos && p > 0) cut = std::min(cut, p);
    }
    return cut == std::string::npos ? s : s.substr(0, cut);
}

static std::string CleanTitleBase(const std::string& raw) {
    std::string s = ReplaceAll(raw, "–", "-");  // en dash
    s = ReplaceAll(s, "—", "-");                // em dash
    s = ReplaceAll(s, "｜", "|");               // thanh dọc full-width
    static const char* const seps[] = {" | ", " // "};
    size_t cut = std::string::npos;
    for (const char* sep : seps) {
        size_t p = s.find(sep);
        if (p != std::string::npos && p > 0) cut = std::min(cut, p);
    }
    if (cut != std::string::npos) s = s.substr(0, cut);
    std::string nb = EraseBrackets(s);
    if (!Trim(nb).empty()) s = nb;
    return TidyText(RemoveNoise(s));
}

static std::string CleanArtist(const std::string& raw) {
    std::string s = EraseBrackets(raw);
    if (Trim(s).empty()) s = raw;
    static const char* const sufs[] = {" - topic", "vevo", " official", " channel"};
    for (int round = 0; round < 2; round++) {
        std::string low = Lower(s);
        for (const char* suf : sufs) {
            size_t n = strlen(suf);
            if (low.size() > n && low.compare(low.size() - n, n, suf) == 0) {
                s.resize(s.size() - n);
                low.resize(low.size() - n);
            }
        }
    }
    s = TidyText(s);
    std::string ls = Lower(s);
    if (ls == "official" || ls == "music" || ls == "topic" || ls == "channel" || ls == "vevo" ||
        ls == "various artists")
        return "";
    return s;
}

typedef std::vector<std::pair<std::string, std::string>> Cands;  // (tên bài, ca sĩ)

static Cands BuildCandidates(const std::string& rawTitle, const std::string& rawArtist) {
    Cands out;
    auto add = [&](std::string t, std::string a) {
        t = TidyText(t);
        a = TidyText(a);
        if (t.empty()) return;
        auto k = std::make_pair(Lower(t), Lower(a));
        for (auto& c : out)
            if (std::make_pair(Lower(c.first), Lower(c.second)) == k) return;
        out.emplace_back(t, a);
    };

    std::string a0 = CleanArtist(rawArtist);
    std::string s = CleanTitleBase(rawTitle);
    size_t dash = s.find(" - ");
    if (dash != std::string::npos) {
        // "Ca sĩ - Tên bài" hay "Tên bài - Ca sĩ"? Dựa vào tên kênh để đoán, rồi thử cả hai.
        std::string L = CutFeat(s.substr(0, dash)), R = CutFeat(s.substr(dash + 3));
        std::string nl = Norm(L), nr = Norm(R), na = Norm(a0);
        auto same = [&](const std::string& x) {
            return !na.empty() && (Contains(x, na) || Contains(na, x));
        };
        if (same(nr) && !same(nl)) {
            add(L, R);
            add(R, L);
        } else {
            add(R, L);
            add(L, R);
        }
    } else {
        add(CutFeat(s), a0);
    }
    if (!out.empty() && !out[0].second.empty()) add(out[0].first, "");  // chỉ tên bài
    add(rawTitle, rawArtist);                                            // nguyên văn (dự phòng)
    return out;
}

// Chạy lần lượt các nguồn lời cho MỘT cặp (tên bài, ca sĩ). Không dùng cache.
static bool SearchAll(const std::string& title, const std::string& artist, LyricsResult& res,
                      std::string& errors) {
    auto store = [&](const LyricsResult&) { return true; };

    // 0a. Zing MP3 (ưu tiên cao nhất, tốt cho nhạc Việt)
    if (g_running) {
        if (FindZing(title, artist, res)) return store(res);
        errors += "Zing MP3 không có lời đồng bộ phù hợp. ";
    }

    // 0a'. NhacCuaTui (nhạc Việt)
    if (g_running) {
        if (FindNct(title, artist, res)) return store(res);
        errors += "NhacCuaTui không có lời đồng bộ phù hợp. ";
    }

    // 0b. NetEase
    if (g_running) {
        if (FindNetease(title, artist, res)) return store(res);
        errors += "NetEase không có lời đồng bộ phù hợp. ";
    }

    // 1. Lyrica
    if (g_running) {
        JV p;
        std::string url = std::string(LYRICA_API) + "?artist=" +
                          UrlEnc(artist.empty() ? "Unknown" : artist) + "&song=" + UrlEnc(title) +
                          "&timestamps=true&fast=true";
        if (HttpGetJson(url, p) && p.t == JV::Obj && p.str("status") == "success") {
            const JV* d = p.get("data");
            if (d && d->t == JV::Obj) {
                Lines lines;
                const JV* timed = d->get("timed_lyrics");
                if (timed) ExtractTimed(*timed, lines, false);
                if (lines.empty()) lines = ParseLrc(d->str("lyrics"));
                if (!lines.empty()) {
                    std::string t = d->str("title"), a = d->str("artist");
                    res.lines = lines;
                    res.status = (t.empty() ? title : t) + " — " + (a.empty() ? artist : a);
                    return store(res);
                }
                errors += "Lyrica không có lời đồng bộ. ";
            } else errors += "Lyrica trả dữ liệu không hợp lệ. ";
        } else errors += "Lyrica không phản hồi/không tìm thấy. ";
    }

    // 2. LRCLIB
    if (g_running) {
        JV p;
        std::string url = "https://lrclib.net/api/search?q=" + UrlEnc(Trim(title + " " + artist));
        if (HttpGetJson(url, p) && p.t == JV::Arr) {
            std::string tt = Norm(title), ta = Norm(artist);
            std::vector<std::pair<double, const JV*>> cand;
            for (const JV& it : p.a) {
                if (it.t != JV::Obj) continue;
                std::string ct = Norm(it.str("trackName")), ca = Norm(it.str("artistName"));
                double ts = Similar(tt, ct);
                if (Contains(tt, ct) || Contains(ct, tt)) ts = std::max(ts, 0.85);
                double as = (!ta.empty() && !ca.empty()) ? Similar(ta, ca) : 0;
                if (Contains(ta, ca) || Contains(ca, ta)) as = std::max(as, 0.85);
                if (ts < 0.65) continue;
                cand.emplace_back(ts * 2 + as, &it);
            }
            std::stable_sort(cand.begin(), cand.end(),
                             [](const std::pair<double, const JV*>& a, const std::pair<double, const JV*>& b) {
                                 return a.first > b.first;
                             });
            for (auto& c : cand) {
                std::string synced = c.second->str("syncedLyrics");
                if (Trim(synced).empty()) continue;
                Lines lines = ParseLrc(synced);
                if (lines.empty()) continue;
                std::string t = c.second->str("trackName"), a = c.second->str("artistName");
                res.lines = lines;
                res.status = (t.empty() ? title : t) + " — " + (a.empty() ? artist : a);
                return store(res);
            }
            errors += "LRCLIB không có lời đồng bộ phù hợp. ";
        } else errors += "LRCLIB không phản hồi. ";
    }

    // 2b. Musixmatch (endpoint desktop không chính thức, token ẩn danh)
    if (g_running) {
        if (FindMusixmatch(title, artist, res)) return store(res);
        errors += "Musixmatch không có lời đồng bộ phù hợp. ";
    }

    // 3. LRCLIB++
    if (g_running) {
        std::string req = "{\"query\":\"";
        auto esc = [](const std::string& s) {
            std::string o;
            for (unsigned char c : s) {
                if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
                else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
            }
            return o;
        };
        req += esc(Trim(title + " " + artist)) + "\",\"song\":\"" + esc(title) + "\",\"artist\":\"" +
               esc(artist) + "\"}";
        std::string body;
        JV p;
        if (HttpFetch(L"POST", LRCLIBPP_API, req, body, 4 * 1024 * 1024, true, false) &&
            ParseJson(body, p)) {
            const JV* data = &p;
            if (p.t == JV::Obj) {
                const JV* d = p.get("data");
                if (!d) d = p.get("lyrics");
                if (d) data = d;
            }
            Lines lines;
            std::string label = title + " — " + artist;
            if (data->t == JV::Str) {
                lines = ParseLrc(data->s);
            } else if (data->t == JV::Obj) {
                const char* fields[] = {"syncedLyrics", "synced_lyrics", "lrc", "lyrics"};
                for (const char* f : fields) {
                    std::string v = data->str(f);
                    if (!Trim(v).empty()) {
                        lines = ParseLrc(v);
                        if (!lines.empty()) {
                            std::string t = data->str("trackName"), a = data->str("artistName");
                            label = (t.empty() ? title : t) + " — " + (a.empty() ? artist : a);
                            break;
                        }
                    }
                }
                if (lines.empty()) {
                    const JV* timed = data->get("timed_lyrics");
                    if (!timed) timed = data->get("lines");
                    if (timed) ExtractTimed(*timed, lines, true);
                }
            }
            if (!lines.empty()) {
                res.lines = lines;
                res.status = label;
                return store(res);
            }
            errors += "LRCLIB++ không có lời đồng bộ. ";
        } else errors += "LRCLIB++ không phản hồi. ";
    }

    // 4. Lyrics.ovh (không đồng bộ)
    if (g_running) {
        JV p;
        std::string url = "https://api.lyrics.ovh/v1/" + UrlEnc(artist.empty() ? "Unknown" : artist) +
                          "/" + UrlEnc(title);
        if (HttpGetJson(url, p) && p.t == JV::Obj) {
            std::string text = p.str("lyrics");
            std::vector<std::string> ls;
            size_t i = 0;
            while (i < text.size()) {
                size_t j = text.find('\n', i);
                if (j == std::string::npos) j = text.size();
                std::string l = Trim(text.substr(i, j - i));
                i = j + 1;
                if (!l.empty()) ls.push_back(l);
            }
            if (!ls.empty()) {
                res.lines.emplace_back(0.0, ls[0]);
                for (size_t k = 1; k < ls.size(); k++) res.lines.emplace_back(4.0 * k, ls[k]);
                res.status = title + " — " + (artist.empty() ? "Unknown" : artist) + " (lời không đồng bộ)";
                return store(res);
            }
        }
        errors += "Lyrics.ovh không tìm thấy lời. ";
    }

    return false;
}

// messy = true cho nguồn YouTube (tiêu đề lộn xộn): làm sạch và thử nhiều cách tách tên bài / ca sĩ.
// videoId (chỉ với nguồn YouTube): tra thẳng lời của YouTube Music trước các nguồn khác.
static LyricsResult FindLyrics(std::string title, std::string artist, bool messy, const std::string& videoId) {
    title = Trim(title);
    artist = Trim(artist);
    LyricsResult res;
    if (title.empty()) {
        res.status = "Không có tên bài hát.";
        return res;
    }
    auto key = std::make_pair(Lower(title), Lower(artist));
    {
        std::lock_guard<std::mutex> lk(g_cacheMtx);
        auto it = g_cache.find(key);
        if (it != g_cache.end()) return it->second;
    }

    if (!videoId.empty() && g_running) {
        LyricsResult r;
        if (FindYtMusic(videoId, title, artist, r)) {
            Wh_Log(L"Lyrics: tìm thấy từ YouTube Music: %s", Widen(r.status).c_str());
            std::lock_guard<std::mutex> lk(g_cacheMtx);
            g_cache[key] = r;
            return r;
        }
    }

    Cands cands;
    if (messy) cands = BuildCandidates(title, artist);
    if (cands.empty()) cands.emplace_back(title, artist);

    std::string firstErrors;
    ULONGLONG t0 = GetTickCount64();
    for (size_t i = 0; i < cands.size() && g_running; i++) {
        if (i > 0 && GetTickCount64() - t0 > 45000) break;  // không kéo dài quá lâu
        Wh_Log(L"Lyrics: thử [%d/%d] \"%s\" / \"%s\"", (int)i + 1, (int)cands.size(),
               Widen(cands[i].first).c_str(), Widen(cands[i].second).c_str());
        LyricsResult r;
        std::string errs;
        if (SearchAll(cands[i].first, cands[i].second, r, errs)) {
            Wh_Log(L"Lyrics: tìm thấy: %s", Widen(r.status).c_str());
            std::lock_guard<std::mutex> lk(g_cacheMtx);
            g_cache[key] = r;
            return r;
        }
        if (i == 0) firstErrors = errs;
    }

    Wh_Log(L"Lyrics: không tìm thấy cho \"%s\" / \"%s\" (%s)", Widen(title).c_str(), Widen(artist).c_str(),
           Widen(firstErrors).c_str());
    res.status = "API thử nghiệm... Chịu khó hát chay bài này nha";
    return res;
}

// ---------------------------------------------------------------------------
// Local HTTP server (receives now-playing data from the browser extension)
// ---------------------------------------------------------------------------

static void SendResp(SOCKET c, int code, const char* text, const std::string& body) {
    std::string h = "HTTP/1.1 " + std::to_string(code) + " " + text +
                    "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    h += body;
    send(c, h.data(), (int)h.size(), 0);
}

static std::string HeaderValue(const std::string& hdr, const std::string& lowerHdr, const char* name) {
    std::string needle = std::string("\r\n") + name + ":";
    size_t p = lowerHdr.find(needle);
    if (p == std::string::npos) return "";
    p += needle.size();
    size_t e = hdr.find("\r\n", p);
    return Trim(hdr.substr(p, e == std::string::npos ? std::string::npos : e - p));
}

static void HandleClient(SOCKET c) {
    DWORD tmo = 2000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));

    std::string req;
    char buf[4096];
    size_t hdrEnd = std::string::npos;
    while (req.size() < 32768) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, n);
        hdrEnd = req.find("\r\n\r\n");
        if (hdrEnd != std::string::npos) break;
    }
    if (hdrEnd == std::string::npos) return;

    std::string hdr = req.substr(0, hdrEnd);
    std::string lh = Lower(hdr);
    size_t sp1 = hdr.find(' ');
    size_t sp2 = sp1 == std::string::npos ? sp1 : hdr.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) { SendResp(c, 400, "Bad Request", "{\"error\":\"bad request\"}"); return; }
    std::string method = hdr.substr(0, sp1);
    std::string path = hdr.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t q = path.find('?');
    if (q != std::string::npos) path.resize(q);

    // Chặn trang web bất kỳ gọi vào server cục bộ: chỉ nhận extension hoặc trang YouTube.
    std::string origin = Lower(HeaderValue(hdr, lh, "origin"));
    if (!origin.empty() && origin.compare(0, 19, "chrome-extension://") != 0 &&
        origin.compare(0, 16, "moz-extension://") != 0 && origin != "https://www.youtube.com" &&
        origin != "https://music.youtube.com") {
        SendResp(c, 403, "Forbidden", "{\"error\":\"forbidden origin\"}");
        return;
    }

    if (method != "POST" || path != "/nowplaying") {
        SendResp(c, 404, "Not Found", "{\"error\":\"not found\"}");
        return;
    }

    long len = atol(HeaderValue(hdr, lh, "content-length").c_str());
    if (len <= 0 || len > 16384) { SendResp(c, 413, "Payload Too Large", "{\"error\":\"invalid body size\"}"); return; }
    std::string body = req.substr(hdrEnd + 4);
    while ((long)body.size() < len) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, n);
    }
    if ((long)body.size() < len) { SendResp(c, 400, "Bad Request", "{\"error\":\"invalid payload\"}"); return; }
    body.resize(len);

    JV d;
    if (!ParseJson(body, d) || d.t != JV::Obj) {
        SendResp(c, 400, "Bad Request", "{\"error\":\"invalid payload\"}");
        return;
    }
    if (d.str("source") != "youtube") { SendResp(c, 400, "Bad Request", "{\"error\":\"unsupported source\"}"); return; }

    std::string title = Trunc(Trim(d.str("title")), 500);
    std::string artist = Trunc(Trim(d.str("artist")), 300);
    std::string url = Trunc(d.str("url"), 2000);
    std::string cover = Trunc(d.str("cover_url"), 2000);

    std::string host;
    if (!HttpsHost(url, host) || !InList(host, PAGE_HOSTS, 2)) {
        SendResp(c, 400, "Bad Request", "{\"error\":\"invalid URL\"}");
        return;
    }
    if (!HttpsHost(cover, host) || !IsCoverHost(host)) cover.clear();
    if (title.empty()) { SendResp(c, 400, "Bad Request", "{\"error\":\"missing title\"}"); return; }

    const JV* pl = d.get("playing");
    NowPlaying np;
    np.valid = true;
    np.source = "youtube";
    np.title = title;
    np.artist = artist;
    np.url = url;
    np.cover = cover;
    np.pos = std::max(0.0, d.num("position", 0));
    np.dur = std::max(0.0, d.num("duration", 0));
    np.playing = (pl && pl->t == JV::Bool) ? pl->b : false;
    np.recvTick = GetTickCount64();
    {
        std::lock_guard<std::mutex> lk(g_stateMtx);
        // Spotify đang phát (dữ liệu còn mới) thì bỏ qua YouTube để hai nguồn không tranh nhau.
        bool spotifyBusy = g_np.source == "spotify" && g_np.playing &&
                           GetTickCount64() - g_np.recvTick < 3000;
        if (!spotifyBusy) g_np = np;
    }
    SendResp(c, 200, "OK", "{\"ok\":true}");
}

static void ServerThread(int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { g_serverFailed = true; return; }
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { g_serverFailed = true; WSACleanup(); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    BOOL excl = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof(excl));
    if (bind(ls, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(ls, 8) != 0) {
        Wh_Log(L"Không thể mở cổng %d", port);
        g_serverFailed = true;
        closesocket(ls);
        WSACleanup();
        return;
    }
    Wh_Log(L"Server lắng nghe tại 127.0.0.1:%d", port);

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(ls, &fds);
        timeval tv{0, 200000};
        int r = select(0, &fds, nullptr, nullptr, &tv);
        if (r <= 0) continue;
        SOCKET c = accept(ls, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        g_clients++;
        std::thread([c]() {
            HandleClient(c);
            closesocket(c);
            g_clients--;
        }).detach();
    }
    closesocket(ls);
    for (int i = 0; i < 60 && g_clients > 0; i++) Sleep(50);
    WSACleanup();
}

// ---------------------------------------------------------------------------
// Spotify via Windows SMTC (System Media Transport Controls)
// ---------------------------------------------------------------------------

// Spotify chỉ cập nhật timeline khi play/pause/seek, nên vị trí được ngoại suy từ LastUpdatedTime.
// Khi Spotify tạm dừng / đóng / đang phát quảng cáo: đóng băng vị trí và đánh dấu "không phát".
static void FreezeSpotify() {
    std::lock_guard<std::mutex> lk(g_stateMtx);
    if (g_np.source == "spotify" && g_np.playing) {
        ULONGLONG now = GetTickCount64();
        g_np.pos += (double)(now - g_np.recvTick) / 1000.0;
        if (g_np.dur > 0) g_np.pos = std::min(g_np.pos, g_np.dur);
        g_np.playing = false;
        g_np.recvTick = now;
    }
}

static bool ReadSmtcThumbnail(
    const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties& props,
    std::vector<BYTE>& out) {
    try {
        auto ref = props.Thumbnail();
        if (!ref) return false;
        auto stream = ref.OpenReadAsync().get();
        uint64_t size = stream.Size();
        if (size == 0 || size > 5000000) return false;
        winrt::Windows::Storage::Streams::DataReader reader(stream);
        reader.LoadAsync((uint32_t)size).get();
        std::vector<uint8_t> buf((size_t)size);
        reader.ReadBytes(buf);
        out = std::move(buf);
        return true;
    } catch (...) {
        return false;
    }
}

static void SmtcThread() {
    using namespace winrt::Windows::Media::Control;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
        Wh_Log(L"SMTC: init_apartment thất bại");
        return;
    }

    GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
    try {
        mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    } catch (...) {
        Wh_Log(L"SMTC: không lấy được session manager");
        winrt::uninit_apartment();
        return;
    }
    Wh_Log(L"SMTC: đã sẵn sàng, đang theo dõi Spotify");

    using dsec = std::chrono::duration<double>;
    std::string lastKey;
    int coverTries = 0;
    bool coverDone = false;

    while (g_running) {
        try {
            GlobalSystemMediaTransportControlsSession sess{nullptr};
            for (auto const& s : mgr.GetSessions()) {
                std::wstring id = s.SourceAppUserModelId().c_str();
                std::transform(id.begin(), id.end(), id.begin(), ::towlower);
                if (id.find(L"spotify") != std::wstring::npos) {
                    sess = s;
                    break;
                }
            }

            if (!sess) {
                FreezeSpotify();  // Spotify đã đóng
            } else {
                auto pb = sess.GetPlaybackInfo();
                bool playing = pb.PlaybackStatus() ==
                               GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                auto rateRef = pb.PlaybackRate();
                double rate = rateRef ? rateRef.Value() : 1.0;
                if (!(rate > 0.1 && rate < 4.0)) rate = 1.0;

                auto props = sess.TryGetMediaPropertiesAsync().get();
                auto tl = sess.GetTimelineProperties();

                std::string title = Trunc(Trim(winrt::to_string(props.Title())), 500);
                std::string artist = Trunc(Trim(winrt::to_string(props.Artist())), 300);

                double dur = std::chrono::duration_cast<dsec>(tl.EndTime() - tl.StartTime()).count();
                double pos = std::chrono::duration_cast<dsec>(tl.Position() - tl.StartTime()).count();
                if (playing) {
                    double el = std::chrono::duration_cast<dsec>(winrt::clock::now() - tl.LastUpdatedTime()).count();
                    if (el < 0 || el > 6 * 3600.0) el = 0;  // dữ liệu thời gian không hợp lệ
                    pos += el * rate;
                }
                if (dur > 0) pos = std::min(pos, dur);
                pos = std::max(0.0, pos);
                dur = std::max(0.0, dur);

                bool isAd = (title == "Advertisement" || title == "Spotify" || artist == "Spotify");

                if (title.empty() || isAd) {
                    FreezeSpotify();
                } else {
                    NowPlaying np;
                    np.valid = true;
                    np.source = "spotify";
                    np.title = title;
                    np.artist = artist;
                    np.url = "spotify:";
                    np.pos = pos;
                    np.dur = dur;
                    np.playing = playing;
                    np.recvTick = GetTickCount64();

                    std::string key = np.title + '\x1f' + np.artist + '\x1f' + np.url;
                    {
                        std::lock_guard<std::mutex> lk(g_stateMtx);
                        // Chỉ ghi khi Spotify đang phát, đang là nguồn hiện tại (để cập nhật pause),
                        // hoặc chưa có nguồn nào.
                        if (playing || g_np.source == "spotify" || !g_np.valid) g_np = np;
                    }

                    // Ảnh bìa: thử lại vài lần vì Spotify đôi khi chưa kịp có thumbnail.
                    if (key != lastKey) {
                        lastKey = key;
                        coverTries = 0;
                        coverDone = false;
                    }
                    if (g_s.showCover && !coverDone && coverTries < 8) {
                        coverTries++;
                        std::vector<BYTE> data;
                        if (ReadSmtcThumbnail(props, data) && !data.empty()) {
                            std::lock_guard<std::mutex> lk(g_spotCoverMtx);
                            g_spotCover.key = key;
                            g_spotCover.data = std::move(data);
                            coverDone = true;
                        }
                    }
                }
            }
        } catch (...) {
            // session biến mất giữa chừng: bỏ qua, thử lại vòng sau
        }

        for (int i = 0; i < 5 && g_running; i++) Sleep(100);
    }

    try {
        winrt::uninit_apartment();
    } catch (...) {
    }
}

// ---------------------------------------------------------------------------
// Overlay window (GDI+ per-pixel alpha layered window)
// ---------------------------------------------------------------------------

struct UI {
    HWND hwnd = nullptr;
    HWND tray = nullptr;
    double dpi = 96;
    unsigned token = 0;
    std::string trackKey;
    std::string coverKey;  // khóa bài đã áp ảnh bìa Spotify
    std::string cardTitle, cardArtist;  // "thẻ tên bài" hiện trước lời
    Lines lines;
    std::string status;
    std::string cur, next;
    Gdiplus::Bitmap* cover = nullptr;
    bool hidden = false;
    bool shown = false;
    bool light = false;
    bool animActive = false;
    ULONGLONG animStart = 0;
    std::string oldCur;
    int lastW = 0, lastH = 0, lastX = INT_MIN, lastY = INT_MIN;
} ui;

typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
static GetDpiForWindow_t g_getDpiForWindow = nullptr;
static const ULONGLONG ANIM_MS = 340;

static double Lerp(double a, double b, double t) { return a + (b - a) * t; }

static double Ease(double t) {
    t = std::clamp(t, 0.0, 1.0);
    double u = 1.0 - t;
    return 1.0 - u * u * u;  // easeOutCubic
}

static bool QueryLightTheme() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return v != 0;
    return false;
}

static void AddRoundRect(Gdiplus::GraphicsPath& gp, float x, float y, float w, float h, float r) {
    float d = r * 2;
    gp.AddArc(x, y, d, d, 180, 90);
    gp.AddArc(x + w - d, y, d, d, 270, 90);
    gp.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    gp.AddArc(x, y + h - d, d, d, 90, 90);
    gp.CloseFigure();
}

// ---- Đo và co/giãn dòng lời để vừa khung ----
struct RowFit {
    float px = 10.0f;  // cỡ chữ (pixel) sẽ dùng
    int lines = 1;     // số dòng sẽ vẽ (1 hoặc 2)
    float h = 14.0f;   // chiều cao cần cho các dòng đó
};

static const float LINE_K = 1.4f;

static float MeasureWidth(Gdiplus::Graphics& g, const std::wstring& wt, const Gdiplus::FontFamily* fam,
                          int style, float px) {
    Gdiplus::Font f(fam, px, (Gdiplus::FontStyle)style, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf;
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    Gdiplus::RectF box;
    g.MeasureString(wt.c_str(), -1, &f, Gdiplus::PointF(0.0f, 0.0f), &sf, &box);
    return box.Width;
}

static int MeasureLines(Gdiplus::Graphics& g, const std::wstring& wt, const Gdiplus::FontFamily* fam,
                        int style, float px, float width) {
    Gdiplus::Font f(fam, px, (Gdiplus::FontStyle)style, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf;
    Gdiplus::RectF layout(0.0f, 0.0f, width, 10000.0f), box;
    INT cp = 0, ln = 0;
    g.MeasureString(wt.c_str(), -1, &f, layout, &sf, &box, &cp, &ln);
    return (int)ln;
}

static RowFit ComputeFit(Gdiplus::Graphics& g, const std::wstring& wt, const Gdiplus::FontFamily* fam,
                         int style, float basePx, float width, int mode, float maxH, bool allowWrap) {
    RowFit r;
    r.px = basePx;
    r.lines = 1;
    r.h = basePx * LINE_K;

    float w = MeasureWidth(g, wt, fam, style, basePx);
    if (w <= width) return r;  // vừa một dòng ở cỡ gốc

    // 1) Thu nhỏ chữ để vừa một dòng (wrap: tối đa còn 80%, shrink: còn 60%).
    float minOne = basePx * (mode == LM_WRAP ? 0.8f : 0.6f);
    float p = std::max(minOne, basePx * (width / w) * 0.98f);
    for (int i = 0; i < 6; i++) {
        if (MeasureWidth(g, wt, fam, style, p) <= width) {
            r.px = p;
            r.h = p * LINE_K;
            return r;
        }
        if (p <= minOne + 0.01f) break;
        p = std::max(minOne, p * 0.96f);
    }

    // 2) Xuống dòng (tối đa 2 dòng), chữ nhỏ dần cho đến khi vừa.
    if (mode == LM_WRAP && allowWrap) {
        for (float q = basePx * 0.85f; q >= basePx * 0.6f - 0.01f; q -= basePx * 0.05f) {
            int n = MeasureLines(g, wt, fam, style, q, width);
            if (n >= 1 && n <= 2 && (float)n * q * LINE_K <= maxH) {
                r.px = q;
                r.lines = n;
                r.h = (float)n * q * LINE_K;
                return r;
            }
        }
        r.px = basePx * 0.6f;  // vẫn dài: 2 dòng chữ nhỏ nhất, phần thừa bị cắt bằng "…"
        r.lines = 2;
        r.h = 2.0f * r.px * LINE_K;
        return r;
    }

    // 3) Không còn cách nào: cỡ nhỏ nhất, phần thừa bị cắt bằng "…"
    r.px = minOne;
    r.h = minOne * LINE_K;
    return r;
}

static RowFit FitRow(Gdiplus::Graphics& g, const std::string& text, const Gdiplus::FontFamily* fam, int style,
                     float basePx, float width, int mode, float maxH, bool allowWrap) {
    RowFit r;
    r.px = basePx;
    r.lines = 1;
    r.h = basePx * LINE_K;
    if (text.empty() || width < 8.0f || mode == LM_ELLIPSIS) return r;

    // Cache kết quả đo để animation 60fps không đo lại mỗi khung hình.
    static std::map<std::string, RowFit> cache;
    std::string key = text + '\x1f' + std::to_string((int)(basePx * 10)) + ',' + std::to_string((int)width) + ',' +
                      std::to_string(mode) + ',' + std::to_string(style) + ',' + (allowWrap ? "w" : "n") + ',' +
                      std::to_string((int)maxH);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 200) cache.clear();

    r = ComputeFit(g, Widen(text), fam, style, basePx, width, mode, maxH, allowWrap);
    cache[key] = r;
    return r;
}

static void DrawRow(Gdiplus::Graphics& g, const std::string& text, const Gdiplus::FontFamily* fam, int style,
                    float px, float x, float y, float w, float h, int alpha, BYTE rgb, bool wrap = false) {
    if (text.empty() || alpha <= 0 || w < 4) return;
    std::wstring wt = Widen(text);
    Gdiplus::Font f(fam, px, (Gdiplus::FontStyle)style, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf;
    sf.SetFormatFlags(wrap ? Gdiplus::StringFormatFlagsLineLimit : Gdiplus::StringFormatFlagsNoWrap);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetAlignment(Gdiplus::StringAlignmentNear);
    sf.SetLineAlignment(Gdiplus::StringAlignmentNear);
    Gdiplus::SolidBrush br(Gdiplus::Color((BYTE)std::min(alpha, 255), rgb, rgb, rgb));
    g.DrawString(wt.c_str(), -1, &f, Gdiplus::RectF(x, y, w, h + (wrap ? 1.0f : 0.0f)), &sf, &br);
}

static void Render() {
    if (!ui.hwnd || !ui.tray) return;
    if (ui.hidden) {
        if (ui.shown) { ShowWindow(ui.hwnd, SW_HIDE); ui.shown = false; }
        return;
    }

    RECT rc{};
    GetClientRect(ui.tray, &rc);
    double sc = ui.dpi / 96.0;
    int W = std::min((int)(g_s.width * sc), (int)(rc.right - rc.left));
    int H = rc.bottom - rc.top;
    if (W < 50 || H < 20) return;
    int X = (int)(g_s.offsetX * sc), Y = (int)(g_s.offsetY * sc);

    bool showCover = g_s.showCover;
    bool showNext = g_s.showNext;
    int cov = std::max(16, std::min(H - (int)std::max(8.0, H * 0.34), (int)(40 * sc)));
    float textX = showCover ? (float)(cov + (int)(10 * sc)) : 0.0f;
    float textW = (float)W - textX;

    float pxCur = (float)(g_s.fontSize * ui.dpi / 72.0);
    float pxNext = (float)(std::max(8, g_s.fontSize - 3) * ui.dpi / 72.0);
    float gapR = (float)(2 * sc);
    float availH = (float)H - (float)(4 * sc);  // chiều cao tối đa cho khối chữ

    double t = 1.0;
    bool anim = ui.animActive;
    if (anim) {
        t = (double)(GetTickCount64() - ui.animStart) / (double)ANIM_MS;
        if (t >= 1.0) { t = 1.0; anim = false; }
    }
    double e = Ease(t);
    BYTE rgb = ui.light ? 0 : 255;

    Gdiplus::FontFamily famSemi(L"Segoe UI Semibold");
    Gdiplus::FontFamily famReg(L"Segoe UI");
    const Gdiplus::FontFamily* famCur = famSemi.IsAvailable() ? &famSemi : &famReg;
    const Gdiplus::FontFamily* famN = &famReg;
    int curStyle = famSemi.IsAvailable() ? Gdiplus::FontStyleRegular : Gdiplus::FontStyleBold;

    HDC sdc = GetDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC mdc = CreateCompatibleDC(sdc);
    HBITMAP hb = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hb || !bits) {
        if (hb) DeleteObject(hb);
        DeleteDC(mdc);
        ReleaseDC(nullptr, sdc);
        return;
    }
    HGDIOBJ old = SelectObject(mdc, hb);

    {
        Gdiplus::Bitmap bmp(W, H, W * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // Ảnh bìa
        if (showCover) {
            float cx = 0, cy = (float)((H - cov) / 2);
            Gdiplus::GraphicsPath cp;
            AddRoundRect(cp, cx + 0.5f, cy + 0.5f, (float)cov - 1, (float)cov - 1, (float)(6 * sc));
            if (ui.cover) {
                float iw = (float)ui.cover->GetWidth(), ih = (float)ui.cover->GetHeight();
                float s = std::min(iw, ih);
                g.SetClip(&cp);
                g.DrawImage(ui.cover, Gdiplus::RectF(cx, cy, (Gdiplus::REAL)cov, (Gdiplus::REAL)cov),
                            (iw - s) / 2, (ih - s) / 2, s, s, Gdiplus::UnitPixel);
                g.ResetClip();
            } else {
                Gdiplus::SolidBrush cb(Gdiplus::Color(60, rgb, rgb, rgb));
                g.FillPath(&cb, &cp);
            }
        }

        // ---- Bố cục chữ: co / xuống dòng theo cài đặt ----
        int mode = g_s.longMode;
        int nextMode = (mode == LM_ELLIPSIS) ? LM_ELLIPSIS : LM_SHRINK;  // dòng kế tiếp luôn một dòng
        RowFit fC = FitRow(g, ui.cur, famCur, curStyle, pxCur, textW, mode, availH, true);
        RowFit fN = FitRow(g, ui.next, famN, Gdiplus::FontStyleRegular, pxNext, textW, nextMode, availH, false);

        float slotC = std::max(pxCur * LINE_K, fC.h);  // ô cố định cho dòng hiện tại (không nhảy khi co chữ)
        float slotN = pxNext * LINE_K;
        bool nextOn = showNext;
        float total = slotC + (nextOn ? gapR + slotN : 0.0f);
        if (nextOn && total > availH + 0.5f) {  // dòng hiện tại xuống dòng -> không đủ chỗ cho dòng kế tiếp
            nextOn = false;
            total = slotC;
        }
        float yCur = ((float)H - total) / 2.0f;
        float yNext = yCur + slotC + gapR;
        float yCurText = yCur + (slotC - fC.h) / 2.0f;
        float yNextText = yNext + (slotN - fN.h) / 2.0f;
        bool wrapC = fC.lines > 1;

        if (anim && t < 1.0) {
            // Dòng cũ kéo lên và mờ dần
            RowFit fO = FitRow(g, ui.oldCur, famCur, curStyle, pxCur, textW, mode, availH, true);
            DrawRow(g, ui.oldCur, famCur, curStyle, fO.px, textX, yCurText - (float)(e * (slotC + gapR)), textW,
                    fO.h, (int)(255 * (1.0 - e)), rgb, fO.lines > 1);
            if (nextOn) {
                // Dòng kế tiếp kéo lên thành dòng hiện tại (to dần, sáng dần)
                RowFit fCN = FitRow(g, ui.cur, famCur, curStyle, pxNext, textW, nextMode, availH, false);
                float yStart = yNext + (slotN - fCN.h) / 2.0f;
                DrawRow(g, ui.cur, famCur, curStyle, (float)Lerp(fCN.px, fC.px, e), textX,
                        (float)Lerp(yStart, yCurText, e), textW, (float)Lerp(fCN.h, fC.h, e),
                        (int)Lerp(150, 255, e), rgb, wrapC);
                // Dòng mới trồi lên từ dưới
                DrawRow(g, ui.next, famN, Gdiplus::FontStyleRegular, fN.px, textX,
                        (float)Lerp(yNextText + slotN + gapR, yNextText, e), textW, fN.h, (int)(150 * e), rgb,
                        false);
            } else {
                DrawRow(g, ui.cur, famCur, curStyle, fC.px, textX, (float)Lerp(yCurText + slotC + gapR, yCurText, e),
                        textW, fC.h, (int)(255 * e), rgb, wrapC);
            }
        } else {
            DrawRow(g, ui.cur, famCur, curStyle, fC.px, textX, yCurText, textW, fC.h, 255, rgb, wrapC);
            if (nextOn)
                DrawRow(g, ui.next, famN, Gdiplus::FontStyleRegular, fN.px, textX, yNextText, textW, fN.h, 150,
                        rgb, false);
        }
    }

    bool geomChanged = X != ui.lastX || Y != ui.lastY || W != ui.lastW || H != ui.lastH;
    if (geomChanged || !ui.shown) {
        SetWindowPos(ui.hwnd, HWND_TOP, X, Y, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ui.lastW = W; ui.lastH = H; ui.lastX = X; ui.lastY = Y;
        ui.shown = true;
    }

    SIZE size{W, H};
    POINT src{0, 0};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(ui.hwnd, sdc, nullptr, &size, mdc, &src, 0, &bf, ULW_ALPHA);

    SelectObject(mdc, old);
    DeleteObject(hb);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
}

static void SetText(const std::string& cur, const std::string& next, bool allowAnim = false) {
    if (cur == ui.cur && next == ui.next && ui.shown) return;
    if (allowAnim && g_s.animate && ui.shown && !ui.hidden && !ui.cur.empty() && cur != ui.cur &&
        cur == ui.next) {
        ui.oldCur = ui.cur;
        ui.animActive = true;
        ui.animStart = GetTickCount64();
        SetTimer(ui.hwnd, 2, 16, nullptr);
    } else {
        ui.animActive = false;
        KillTimer(ui.hwnd, 2);
    }
    ui.cur = cur;
    ui.next = next;
    Render();
}

static void SpawnLyricsWorker(const std::string& title, const std::string& artist, unsigned token,
                              bool messy, const std::string& videoId) {
    HWND hw = ui.hwnd;
    g_workers++;
    std::thread([=]() {
        LyricsResult r = FindLyrics(title, artist, messy, videoId);
        if (g_running) {
            LyricsMsg* m = new LyricsMsg{token, r};
            if (!PostMessageW(hw, WM_LYRICS_DONE, 0, (LPARAM)m)) delete m;
        }
        g_workers--;
    }).detach();
}

static void SpawnCoverWorker(const std::string& url, unsigned token) {
    HWND hw = ui.hwnd;
    g_workers++;
    std::thread([=]() {
        std::string host, data;
        if (g_running && HttpsHost(url, host) && IsCoverHost(host) &&
            HttpFetch(L"GET", url, "", data, 5000000, false, true)) {
            CoverMsg* m = new CoverMsg{token, std::vector<BYTE>(data.begin(), data.end())};
            if (!PostMessageW(hw, WM_COVER_DONE, 0, (LPARAM)m)) delete m;
        }
        g_workers--;
    }).detach();
}

// Giải mã ảnh bìa từ bytes (JPEG/PNG...) và thay ảnh hiện tại. Trả về true nếu thành công.
static bool SetCoverBytes(const std::vector<BYTE>& data) {
    if (data.empty()) return false;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, data.size());
    if (!hg) return false;
    void* mem = GlobalLock(hg);
    if (!mem) { GlobalFree(hg); return false; }
    memcpy(mem, data.data(), data.size());
    GlobalUnlock(hg);
    IStream* st = nullptr;
    if (CreateStreamOnHGlobal(hg, TRUE, &st) != S_OK) { GlobalFree(hg); return false; }
    bool ok = false;
    {
        Gdiplus::Bitmap src(st, FALSE);
        if (src.GetLastStatus() == Gdiplus::Ok && src.GetWidth() > 0 && src.GetHeight() > 0) {
            Gdiplus::Bitmap* copy = new Gdiplus::Bitmap((INT)src.GetWidth(), (INT)src.GetHeight(),
                                                        PixelFormat32bppPARGB);
            {
                Gdiplus::Graphics g(copy);
                g.DrawImage(&src, 0, 0, (INT)src.GetWidth(), (INT)src.GetHeight());
            }
            delete ui.cover;
            ui.cover = copy;
            ok = true;
        }
    }
    st->Release();
    if (ok) Render();
    return ok;
}

static void StartTrack(const NowPlaying& np, bool reloadCover) {
    ui.token++;
    ui.lines.clear();
    ui.status.clear();
    ui.hidden = false;

    // Thẻ tên bài (hiện trong vài giây đầu): Spotify đã sạch sẵn, YouTube cần làm sạch.
    if (np.source == "spotify") {
        ui.cardTitle = np.title;
        ui.cardArtist = np.artist;
    } else {
        ui.cardTitle = CleanTitleBase(np.title);
        if (ui.cardTitle.empty()) ui.cardTitle = np.title;
        ui.cardArtist = CleanArtist(np.artist);
    }
    if (!ui.cardArtist.empty() && Contains(Norm(ui.cardTitle), Norm(ui.cardArtist))) ui.cardArtist.clear();
    if (reloadCover) {
        delete ui.cover;
        ui.cover = nullptr;
        ui.coverKey.clear();
    }
    SetText("Đang tìm lời bài hát…", "");
    Render();
    if (reloadCover && !np.cover.empty()) SpawnCoverWorker(np.cover, ui.token);
    SpawnLyricsWorker(np.title, np.artist, ui.token, np.source != "spotify",
                      np.source == "youtube" ? ExtractVideoId(np.url) : std::string());
}

// Ảnh bìa Spotify do thread SMTC cung cấp: áp khi đã khớp bài hiện tại.
static void TrySpotifyCover(const std::string& key) {
    if (ui.coverKey == key) return;
    std::vector<BYTE> data;
    {
        std::lock_guard<std::mutex> lk(g_spotCoverMtx);
        if (g_spotCover.key != key || g_spotCover.data.empty()) return;
        data = g_spotCover.data;
    }
    if (SetCoverBytes(data)) ui.coverKey = key;
}

static void OnTick() {
    NowPlaying np;
    {
        std::lock_guard<std::mutex> lk(g_stateMtx);
        np = g_np;
    }
    if (!np.valid) {
        if (g_serverFailed)
            SetText("Không mở được cổng " + std::to_string(g_s.port) + " (đã bị chiếm?)", "");
        else
            SetText("Đang chờ bài hát…", "");
        return;
    }

    std::string key = np.title + '\x1f' + np.artist + '\x1f' + np.url;
    if (key != ui.trackKey) {
        ui.trackKey = key;
        StartTrack(np, true);
    }
    if (np.source == "spotify" && g_s.showCover) TrySpotifyCover(key);

    double pos = np.pos;
    if (np.playing) pos += (double)(GetTickCount64() - np.recvTick) / 1000.0;
    if (np.dur > 0) pos = std::min(pos, np.dur);

    // Vài giây đầu của bài: hiện tên bài + ca sĩ trước khi hiện lời.
    bool titleCard = g_s.titleSeconds > 0 && pos < (double)g_s.titleSeconds && !ui.cardTitle.empty();
    if (titleCard) {
        SetText(ui.cardTitle, ui.cardArtist);
    } else if (!ui.lines.empty()) {
        int idx = -1;
        for (size_t i = 0; i < ui.lines.size(); i++) {
            if (ui.lines[i].first <= pos) idx = (int)i;
            else break;
        }
        auto txt = [&](int i) {
            return ui.lines[i].second.empty() ? std::string("♪") : ui.lines[i].second;
        };
        if (idx >= 0)
            SetText(txt(idx), idx + 1 < (int)ui.lines.size() ? txt(idx + 1) : "", true);
        else
            SetText("♪", txt(0), true);
    } else if (!ui.status.empty()) {
        SetText(ui.status, "");
    } else {
        SetText("Đang tìm lời bài hát…", "");
    }

    // Giữ lời nằm trên cùng trong taskbar, theo dõi theme / DPI / kích thước taskbar.
    static ULONGLONG lastKeep = 0;
    ULONGLONG nowT = GetTickCount64();
    if (nowT - lastKeep > 500) {
        lastKeep = nowT;
        bool dirty = false;
        if (ui.shown) SetWindowPos(ui.hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        bool light = QueryLightTheme();
        if (light != ui.light) { ui.light = light; dirty = true; }
        if (g_getDpiForWindow) {
            double d = g_getDpiForWindow(ui.tray);
            if (d > 0 && d != ui.dpi) { ui.dpi = d; dirty = true; }
        }
        RECT rc{};
        GetClientRect(ui.tray, &rc);
        if (ui.shown && (rc.bottom - rc.top != ui.lastH)) dirty = true;
        if (dirty) Render();
    }
}

static void ApplyCover(CoverMsg* m) {
    if (m->token != ui.token || m->data.empty()) return;
    SetCoverBytes(m->data);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TIMER:
            if (wp == 1) {
                OnTick();
            } else if (wp == 2) {
                if (GetTickCount64() - ui.animStart >= ANIM_MS) {
                    ui.animActive = false;
                    KillTimer(hwnd, 2);
                }
                Render();
            }
            return 0;
        case WM_LYRICS_DONE: {
            LyricsMsg* m = (LyricsMsg*)lp;
            if (m->token == ui.token) {
                ui.lines = m->res.lines;
                ui.status = m->res.status;
                ui.cur.clear();  // ép vẽ lại
                OnTick();        // OnTick quyết định hiện thẻ tên bài / lời / thông báo lỗi
            }
            delete m;
            return 0;
        }
        case WM_COVER_DONE: {
            CoverMsg* m = (CoverMsg*)lp;
            ApplyCover(m);
            delete m;
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Tìm lại lời bài hát");
            AppendMenuW(menu, MF_STRING, 2, L"Ẩn đến bài tiếp theo");
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                                     pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (cmd == 1) {
                NowPlaying np;
                {
                    std::lock_guard<std::mutex> lk(g_stateMtx);
                    np = g_np;
                }
                if (np.valid) {
                    {
                        std::lock_guard<std::mutex> lk(g_cacheMtx);
                        g_cache.erase({Lower(Trim(np.title)), Lower(Trim(np.artist))});
                    }
                    StartTrack(np, false);
                }
            } else if (cmd == 2) {
                ui.hidden = true;
                Render();
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Main thread (owns the window + message loop)
// ---------------------------------------------------------------------------

static void MainThread() {
    // Chỉ chạy trong tiến trình explorer.exe đang sở hữu taskbar.
    HWND tray = nullptr;
    for (int i = 0; i < 120 && g_running; i++) {
        tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) break;
        Sleep(500);
    }
    if (!tray || !g_running) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(tray, &pid);
    if (pid != GetCurrentProcessId()) return;

    std::thread server(ServerThread, g_s.port);
    std::thread smtc;
    if (g_s.spotify) smtc = std::thread(SmtcThread);

    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gtoken = 0;
    Gdiplus::GdiplusStartup(&gtoken, &gsi, nullptr);

    {
        g_getDpiForWindow =
            (GetDpiForWindow_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        ui.tray = tray;
        ui.dpi = g_getDpiForWindow ? (double)g_getDpiForWindow(tray) : 96.0;
        ui.light = QueryLightTheme();

        WNDCLASSW wc{};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"AutoLyricsOverlayWnd_WH";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);

        HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE, wc.lpszClassName, L"Auto Lyrics",
                                    WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, tray, nullptr, wc.hInstance, nullptr);
        if (hwnd) {
            ui.hwnd = hwnd;
            g_hwnd = hwnd;
            SetTimer(hwnd, 1, 100, nullptr);
            SetText("Đang chờ bài hát…", "");
            if (!g_running) DestroyWindow(hwnd);

            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            g_hwnd = nullptr;
            ui.hwnd = nullptr;
        }
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }

    g_running = false;  // dừng server + SMTC + worker nếu cửa sổ tự đóng
    if (server.joinable()) server.join();
    if (smtc.joinable()) smtc.join();

    for (int i = 0; i < 160 && g_workers > 0; i++) Sleep(50);  // tối đa ~8s

    delete ui.cover;
    ui.cover = nullptr;
    Gdiplus::GdiplusShutdown(gtoken);
}

// ---------------------------------------------------------------------------
// Windhawk entry points
// ---------------------------------------------------------------------------

static void LoadSettings() {
    g_s.port = Wh_GetIntSetting(L"port");
    g_s.fontSize = Wh_GetIntSetting(L"fontSize");
    g_s.width = Wh_GetIntSetting(L"width");
    g_s.offsetX = Wh_GetIntSetting(L"offsetX");
    g_s.offsetY = Wh_GetIntSetting(L"offsetY");
    g_s.spotify = Wh_GetIntSetting(L"spotify") != 0;
    g_s.showCover = Wh_GetIntSetting(L"showCover") != 0;
    g_s.showNext = Wh_GetIntSetting(L"showNext") != 0;
    g_s.animate = Wh_GetIntSetting(L"animate") != 0;
    g_s.titleSeconds = Wh_GetIntSetting(L"titleSeconds");

    PCWSTR lm = Wh_GetStringSetting(L"longLines");
    std::wstring lms = lm ? lm : L"";
    if (lm) Wh_FreeStringSetting(lm);
    g_s.longMode = (lms == L"ellipsis") ? LM_ELLIPSIS : (lms == L"shrink") ? LM_SHRINK : LM_WRAP;

    if (g_s.port < 1024 || g_s.port > 65535) g_s.port = 8765;
    g_s.fontSize = std::clamp(g_s.fontSize, 8, 24);
    g_s.width = std::clamp(g_s.width, 120, 1200);
    g_s.offsetX = std::clamp(g_s.offsetX, 0, 4000);
    g_s.offsetY = std::clamp(g_s.offsetY, -200, 200);
    g_s.titleSeconds = std::clamp(g_s.titleSeconds, 0, 30);
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_running = true;
    g_serverFailed = false;
    g_mainThread = std::thread(MainThread);
    return TRUE;
}

void Wh_ModUninit() {
    g_running = false;
    HWND h = g_hwnd;
    if (h) PostMessageW(h, WM_CLOSE, 0, 0);
    if (g_mainThread.joinable()) g_mainThread.join();
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    *bReload = TRUE;  // đổi cổng/giao diện -> nạp lại mod cho sạch
    return TRUE;
}
