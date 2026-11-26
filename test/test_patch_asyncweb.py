"""Testes do remendo aplicado à biblioteca ESPAsyncWebServer."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
from patch_asyncweb import PATCHES, patch_library  # noqa: E402


def fake_library(tmp):
    src = Path(tmp) / "src"
    src.mkdir()
    for name, pairs in PATCHES.items():
        (src / name).write_text("// antes\n" + "".join(old for old, _ in pairs) + "// depois\n",
                                encoding="utf-8")
    return Path(tmp)


class PatchAsyncWeb(unittest.TestCase):
    def test_aplica_uma_vez_so(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = fake_library(tmp)
            self.assertTrue(patch_library(lib))
            for name, pairs in PATCHES.items():
                text = (lib / "src" / name).read_text(encoding="utf-8")
                for old, new in pairs:
                    self.assertIn(new, text)
                    self.assertNotIn(old, text)
            self.assertFalse(patch_library(lib))

    def test_codigo_diferente_e_erro_e_nada_e_gravado(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = fake_library(tmp)
            names = list(PATCHES)
            (lib / "src" / names[-1]).write_text("outra versao\n", encoding="utf-8")
            before = (lib / "src" / names[0]).read_text(encoding="utf-8")
            with self.assertRaises(ValueError):
                patch_library(lib)
            self.assertEqual((lib / "src" / names[0]).read_text(encoding="utf-8"), before)


if __name__ == "__main__":
    unittest.main()
