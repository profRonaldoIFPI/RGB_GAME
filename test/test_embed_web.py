"""Testes do script que embute as páginas. Rode: python3 -m unittest discover -s test -p 'test_*.py'"""
import sys
import tempfile
import unicodedata
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
from embed_web import DELIM, generate, render_header  # noqa: E402


class EmbedWeb(unittest.TestCase):
    def test_html_vai_sem_alteracoes(self):
        html = '<p class="a">João \\n \\\\ \'x\' )"</p>\n<script>let s="?";</script>\n'
        out = render_header({"PAGE_INDEX": html})
        self.assertIn(f'const char PAGE_INDEX[] PROGMEM = R"{DELIM}({html}){DELIM}";', out)

    def test_delimitador_no_html_e_erro(self):
        with self.assertRaises(ValueError):
            render_header({"PAGE_INDEX": f'x){DELIM}" y'})

    def _root(self, tmp):
        root = Path(tmp)
        (root / "web").mkdir()
        (root / "web" / "index.html").write_text("<p>1</p>", encoding="utf-8")
        (root / "web" / "admin.html").write_text("<p>2</p>", encoding="utf-8")
        return root

    def test_so_regrava_quando_muda(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self._root(tmp)
            self.assertTrue(generate(root))
            self.assertIn("PAGE_ADMIN", (root / "include" / "web_pages.h").read_text(encoding="utf-8"))
            self.assertFalse(generate(root))
            (root / "web" / "index.html").write_text("<p>3</p>", encoding="utf-8")
            self.assertTrue(generate(root))

    def test_html_ausente_e_erro(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self._root(tmp)
            (root / "web" / "admin.html").unlink()
            with self.assertRaises(FileNotFoundError):
                generate(root)

    def test_paginas_sem_caracteres_invisiveis(self):
        web = Path(__file__).resolve().parent.parent / "web"
        for page in web.glob("*.html"):
            bad = [hex(ord(c)) for c in page.read_text(encoding="utf-8")
                   if unicodedata.category(c) in ("Mn", "Cf", "Cc") and c not in "\n\t"]
            self.assertEqual(bad, [], page.name)


if __name__ == "__main__":
    unittest.main()
