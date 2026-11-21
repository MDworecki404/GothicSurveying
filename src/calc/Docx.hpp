// Minimalny zapis .docx bez zależności: archiwum ZIP (metoda "stored", CRC32) + WordprocessingML (bez Gothic API).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "FieldBook.hpp"
#include "ManualCheck.hpp"

inline uint32_t Crc32(const std::string& bytes) {
  static uint32_t table[256]; static bool ready = false;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; }
    ready = true;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for (unsigned char b : bytes) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

namespace docxdetail {
inline void P16(std::string& s, uint32_t v) { s += (char)(v & 0xFF); s += (char)((v >> 8) & 0xFF); }
inline void P32(std::string& s, uint32_t v) { P16(s, v & 0xFFFF); P16(s, v >> 16); }

inline std::string Esc(const std::string& t) {
  std::string o;
  for (char c : t) {
    switch (c) { case '&': o += "&amp;"; break; case '<': o += "&lt;"; break; case '>': o += "&gt;"; break; case '"': o += "&quot;"; break; default: o += c; }
  }
  return o;
}
inline std::string Para(const std::string& text, bool bold = false, int halfPts = 0, bool center = false) {
  std::string o = "<w:p>";
  if (center) o += "<w:pPr><w:jc w:val=\"center\"/></w:pPr>";
  o += "<w:r>";
  if (bold || halfPts) { o += "<w:rPr>"; if (bold) o += "<w:b/>"; if (halfPts) o += "<w:sz w:val=\"" + std::to_string(halfPts) + "\"/>"; o += "</w:rPr>"; }
  return o + "<w:t xml:space=\"preserve\">" + Esc(text) + "</w:t></w:r></w:p>";
}
inline std::string Row(const std::vector<std::string>& cells, bool header, int widthDxa = 1500) {
  std::string o = "<w:tr>";
  for (const std::string& c : cells)
    o += "<w:tc><w:tcPr><w:tcW w:w=\"" + std::to_string(widthDxa) + "\" w:type=\"dxa\"/></w:tcPr>" + Para(c, header) + "</w:tc>";
  return o + "</w:tr>";
}
}  // namespace docxdetail

// files: (nazwa, zawartość); każdy wpis zapisany bez kompresji.
inline std::string ZipStore(const std::vector<std::pair<std::string, std::string>>& files) {
  using namespace docxdetail;
  std::string out, central;
  for (const auto& f : files) {
    const std::string& name = f.first; const std::string& data = f.second;
    uint32_t offset = (uint32_t)out.size(), crc = Crc32(data);
    P32(out, 0x04034b50); P16(out, 20); P16(out, 0); P16(out, 0); P16(out, 0); P16(out, 0x21);   // wersja, flagi, metoda=0, czas, data=1980-01-01
    P32(out, crc); P32(out, (uint32_t)data.size()); P32(out, (uint32_t)data.size()); P16(out, (uint32_t)name.size()); P16(out, 0);
    out += name; out += data;
    P32(central, 0x02014b50); P16(central, 20); P16(central, 20); P16(central, 0); P16(central, 0); P16(central, 0); P16(central, 0x21);
    P32(central, crc); P32(central, (uint32_t)data.size()); P32(central, (uint32_t)data.size());
    P16(central, (uint32_t)name.size()); P16(central, 0); P16(central, 0); P16(central, 0); P16(central, 0); P32(central, 0); P32(central, offset);
    central += name;
  }
  uint32_t cdOffset = (uint32_t)out.size();
  out += central;
  P32(out, 0x06054b50); P16(out, 0); P16(out, 0); P16(out, (uint32_t)files.size()); P16(out, (uint32_t)files.size());
  P32(out, (uint32_t)central.size()); P32(out, cdOffset); P16(out, 0);
  return out;
}

namespace docxdetail {
// Pakiet .docx z gotowego XML-a treści (strona A4 pozioma).
inline std::string WrapDocx(const std::string& body) {
  const std::string doc =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
      "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>" + body +
      "<w:sectPr><w:pgSz w:w=\"16838\" w:h=\"11906\" w:orient=\"landscape\"/>"
      "<w:pgMar w:top=\"1134\" w:right=\"1134\" w:bottom=\"1134\" w:left=\"1134\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/></w:sectPr></w:body></w:document>";
  const std::string types =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
      "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
      "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
      "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
      "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/></Types>";
  const std::string rels =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
      "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
      "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>";
  return ZipStore({{"[Content_Types].xml", types}, {"_rels/.rels", rels}, {"word/document.xml", doc}});
}
}  // namespace docxdetail

inline std::string BuildDocx(const FieldBook& fb, const std::string& dateText, const ManualVerdict* verdict = nullptr) {
  using namespace docxdetail; using namespace fbdetail;
  const bool closure = fb.sum.hasClosure;
  std::string body;
  body += Para("Dziennik niwelacji", true, 36, true);
  body += Para(KindName(fb.kind) + ", liczenie: " + ModeName(fb.mode) + ", data: " + dateText +
               (fb.complete ? "" : " (dziennik niekompletny - robota przerwana)"));
  std::string tbl = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
  for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
    tbl += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
  tbl += "</w:tblBorders></w:tblPr><w:tblGrid>";
  for (int i = 0; i < 14; i++) tbl += "<w:gridCol w:w=\"1100\"/>";
  tbl += "</w:tblGrid>";
  tbl += Row({"Nr stan.", "Punkt", "Odczyt wstecz [mm]", "Odczyt wprzód [mm]", "Δh [mm]", "Poprawka v [mm]", "Δh wyrównane [mm]", "H [m]", "Uwagi",
              "Celowa wstecz [m]", "Celowa wprzód [m]", "Odczyt wstecz II [mm]", "Odczyt wprzód II [mm]", "Δh I−II [mm]"}, true);
  tbl += Row({"0", fb.startName, "", "", "", "", "", Num(fb.startHeightM, 3), "reper", "", "", "", "", ""}, false);
  for (const FbRow& r : fb.rows)
    tbl += Row({std::to_string(r.standNo), r.pointName, std::to_string(r.backMm), std::to_string(r.foreMm), Num(r.dhMm, 1),
                closure ? Num(r.vMm, 1) : "", closure ? Num(r.dhAdjMm, 1) : "", Num(r.heightM, 3), r.note,
                r.stadiaBackCm >= 0 ? Num(r.stadiaBackCm / 100.0, 1) : "", r.stadiaForeCm >= 0 ? Num(r.stadiaForeCm / 100.0, 1) : "",
                ObsadzkaCells(r)[0], ObsadzkaCells(r)[1], ObsadzkaCells(r)[2]}, false);
  body += tbl + "</w:tbl>";
  body += Para("H reper A = " + Num(fb.startHeightM, 3) + " m" + (fb.kind == TraverseKind::Anchored && fb.hasEnd ? ", H reper B = " + Num(fb.endHeightM, 3) + " m" : ""));
  body += Para("Σ wstecz = " + Num(fb.sum.sumBackMm, 1) + " mm, Σ wprzód = " + Num(fb.sum.sumForeMm, 1) + " mm, ΣΔh = " + Num(fb.sum.sumDhMm, 1) +
               " mm, długość ciągu L = " + Num(fb.sum.lengthM, 1) + " m");
  body += Para(closure ? "fh = " + Num(fb.sum.fhMm, 1) + " mm, fh dop. = " + Num(fb.sum.fhDopMm, 1) + " mm, wynik: " +
                             (fb.sum.withinTolerance ? "w tolerancji" : "przekroczony")
                       : std::string("Ciąg otwarty - brak kontroli (błędu zamknięcia)."), true);
  if (verdict) {                                         // tryb samodzielny: tabela „Sprawdzenie”
    body += Para("Sprawdzenie (tryb samodzielny): trafione " + std::to_string(verdict->ok) + " z " + std::to_string(verdict->total), true);
    std::string t = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
    for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
      t += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
    t += "</w:tblBorders></w:tblPr><w:tblGrid>";
    for (int i = 0; i < 5; i++) t += "<w:gridCol w:w=\"1500\"/>";
    t += "</w:tblGrid>" + Row({"Komórka", "Poprawna", "Twoja", "Różnica [mm]", "Ocena"}, true);
    for (const auto& r : VerdictRows(*verdict)) t += Row(r, false);
    body += t + "</w:tbl>" + Para("");
  }
  return docxdetail::WrapDocx(body);
}
