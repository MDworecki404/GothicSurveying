"""Sprawdza wygenerowany .docx: poprawny ZIP, poprawny XML, jedna tabela, oczekiwane teksty. Uzycie: check_docx.py plik.docx"""
import sys
import zipfile
from xml.dom import minidom

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
path = sys.argv[1]
z = zipfile.ZipFile(path)
assert z.testzip() is None, "uszkodzone CRC w ZIP"
for name in z.namelist():
    if name.endswith(".xml") or name.endswith(".rels"):
        minidom.parseString(z.read(name))                      # rzuca wyjatek przy niepoprawnym XML
doc = minidom.parseString(z.read("word/document.xml"))
tables = doc.getElementsByTagNameNS(W, "tbl")
assert len(tables) == 1, f"oczekiwano 1 tabeli, jest {len(tables)}"
rows = tables[0].getElementsByTagNameNS(W, "tr")
assert len(rows) == 3 + 2, f"oczekiwano 5 wierszy (naglowek + start + 3 stanowiska), jest {len(rows)}"
text = "".join(t.firstChild.data for t in doc.getElementsByTagNameNS(W, "t") if t.firstChild)
for needle in ("Dziennik niwelacji", "ciąg dowiązany", "Zabka & <1> żółć", "2026-11-20", "przekroczony"):
    assert needle in text, f"brak tekstu: {needle}"
assert 'w:orient="landscape"' in z.read("word/document.xml").decode("utf-8")
print("check_docx OK")
