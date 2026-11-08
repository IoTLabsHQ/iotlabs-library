#include "LinuxHttpClient.h"

#if !defined(ARDUINO)

#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// curl_global_init()/curl_global_cleanup() must each run exactly once per
// process (not safely reentrant/thread-safe to call repeatedly) -- same
// reasoning as libmosquitto's lib_init/lib_cleanup in LinuxMqttTransport,
// ref-counted here via a process-wide counter since more than one
// LinuxHttpClient could theoretically be constructed.
int g_curlRefCount = 0;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

// Securely-created temp file holding a PEM blob, auto-removed on scope
// exit. Used both as LinuxHttpClient's CAINFO fallback (this file) and by
// LinuxMqttTransport for its cafile parameter (libmosquitto's TLS API is
// file-path-only, no in-memory blob support at all) -- the two adapters
// each keep their own small copy rather than share a header, per the
// task's explicit "duplication is fine, keeps files self-contained" call.
// Constructing with pem == nullptr/"" is a valid, deliberate no-op (keeps
// call sites branch-free).
class ScopedPemTempFile {
 public:
  explicit ScopedPemTempFile(const char* pem) {
    if (!pem || pem[0] == '\0') return;
    const char* tmpdir = std::getenv("TMPDIR");
    if (!tmpdir || tmpdir[0] == '\0') tmpdir = "/tmp";
    std::string tmpl = std::string(tmpdir) + "/iotlabs_http_ca_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = ::mkstemp(buf.data());
    if (fd < 0) return;
    ::fchmod(fd, 0600);
    const size_t len = std::strlen(pem);
    const bool ok = ::write(fd, pem, len) == static_cast<ssize_t>(len);
    ::close(fd);
    if (ok) _path = buf.data();
  }

  ~ScopedPemTempFile() {
    if (!_path.empty()) ::unlink(_path.c_str());
  }

  ScopedPemTempFile(const ScopedPemTempFile&) = delete;
  ScopedPemTempFile& operator=(const ScopedPemTempFile&) = delete;

  bool valid() const { return !_path.empty(); }
  const std::string& path() const { return _path; }

 private:
  std::string _path;
};

}  // namespace

LinuxHttpClient::LinuxHttpClient() {
  if (g_curlRefCount == 0) curl_global_init(CURL_GLOBAL_DEFAULT);
  ++g_curlRefCount;
}

LinuxHttpClient::~LinuxHttpClient() {
  --g_curlRefCount;
  if (g_curlRefCount == 0) curl_global_cleanup();
}

bool LinuxHttpClient::postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure,
                                const char* rootCA, IoTLabsHttpResponse& outResponse) {
  if (!url) return false;

  CURL* curl = curl_easy_init();
  if (!curl) return false;

  std::string responseBody;
  struct curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody ? jsonBody : "");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonBody ? static_cast<long>(std::strlen(jsonBody)) : 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  if (tlsInsecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  const bool wantCA = !tlsInsecure && rootCA && rootCA[0] != '\0';

  // CA cert handling: `rootCA` is PEM *content*, not a path (same convention
  // as the ESP32 adapter's WiFiClientSecure::setCACert()). CURLOPT_CAINFO_BLOB
  // (libcurl >= 7.77) accepts that content directly in memory; this dev
  // machine runs curl 8.7.1 (confirmed via `curl-config --version`), so the
  // blob branch below compiles and is the one actually exercised on this
  // build. Against an older libcurl this falls back at COMPILE TIME (via the
  // installed libcurl's own version macro, not an untested #ifdef) to
  // writing the PEM into a securely-created, 0600, auto-removed temp file
  // and using CURLOPT_CAINFO with that path.
#if defined(CURLOPT_CAINFO_BLOB) && LIBCURL_VERSION_NUM >= 0x074D00
  curl_blob blob;
  if (wantCA) {
    blob.data = const_cast<char*>(rootCA);
    blob.len = std::strlen(rootCA);
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
  }
#else
  ScopedPemTempFile pemTempFile(wantCA ? rootCA : nullptr);
  if (pemTempFile.valid()) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, pemTempFile.path().c_str());
  }
#endif

  const CURLcode res = curl_easy_perform(curl);

  long statusCode = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) return false;

  outResponse.statusCode = static_cast<int>(statusCode);
  outResponse.body = responseBody;
  return true;
}

#endif  // !ARDUINO
