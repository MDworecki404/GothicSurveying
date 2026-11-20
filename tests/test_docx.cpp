#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "calc/Docx.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static uint16_t u16(const std::string& s, size_t p) { return (uint8_t)s[p] | ((uint8_t)s[p + 1] << 8); }

static void crc32_known_value() { assert(Crc32("123456789") == 0xCBF43926u); }

static void zip_store_has_headers_and_entry_count() {
  std::string z = ZipStore({{"a.txt", "hello"}, {"b/c.txt", "world"}});
  assert(z.rfind("PK\x03\x04", 0) == 0);
  size_t eocd = z.size() - 22;
  assert(z.compare(eocd, 4, "PK\x05\x06") == 0);
  assert(u16(z, eocd + 10) == 2);
}

static FieldBook Sample() {
  std::vector<Stand> s = { {1500, 1200, 3000, 3000}, {1400, 1650, 3000, 3000}, {1300, 1100, 3000, 3000} };
  return BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, s, {"Zabka & <1> żółć", "2", "B"}, 100.000, 100.240, true);
}

static void docx_contains_required_parts() {
  std::string d = BuildDocx(Sample(), "2026-11-20");
  assert(has(d, "[Content_Types].xml") && has(d, "_rels/.rels") && has(d, "word/document.xml"));
  assert(has(d, "w:orient=\"landscape\""));
  assert(has(d, "Zabka &amp; &lt;1&gt; żółć"));          // znaki XML ucieczkowane, polskie zostają
}

static void writes_sample_file_for_python_check() {
  std::filesystem::create_directories("tests/out");
  std::string d = BuildDocx(Sample(), "2026-11-20");
  std::ofstream f("tests/out/sample.docx", std::ios::binary);
  f.write(d.data(), (std::streamsize)d.size());
  assert(f.good());
}

int main() {
  crc32_known_value(); zip_store_has_headers_and_entry_count(); docx_contains_required_parts(); writes_sample_file_for_python_check();
  std::puts("test_docx OK");
  return 0;
}
