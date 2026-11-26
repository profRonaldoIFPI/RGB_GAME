"""Embute as páginas HTML (pasta web/) no firmware.

O PlatformIO roda este script ANTES de cada build (veja extra_scripts no
platformio.ini). Ele lê web/*.html e gera include/web_pages.h, com cada página
dentro de um texto C++. Edite sempre os .html; nunca o arquivo gerado.

Também dá para rodar à mão:  python3 scripts/embed_web.py
"""
from pathlib import Path

# Nome da variável C++  ->  arquivo HTML
PAGES = {"PAGE_INDEX": "index.html", "PAGE_ADMIN": "admin.html"}

# Marca o começo e o fim do texto "cru" do C++:  R"RGBPAGE( ... )RGBPAGE"
DELIM = "RGBPAGE"


def render_header(pages):
    """Recebe {nome_da_variavel: html} e devolve o texto do cabeçalho C++."""
    parts = [
        "// ARQUIVO GERADO por scripts/embed_web.py - NÃO EDITE.",
        "// Altere os arquivos da pasta web/ e compile de novo.",
        "#pragma once",
        "#include <pgmspace.h>",
        "",
    ]
    for name, html in pages.items():
        if f"){DELIM}\"" in html:
            raise ValueError(f"{name}: o HTML não pode conter o texto ){DELIM}\"")
        parts.append(f'const char {name}[] PROGMEM = R"{DELIM}({html}){DELIM}";')
        parts.append("")
    return "\n".join(parts)


def generate(root):
    """Gera include/web_pages.h. Só regrava se mudou. Devolve True se gravou."""
    root = Path(root)
    pages = {name: (root / "web" / file).read_text(encoding="utf-8")
             for name, file in PAGES.items()}
    header = render_header(pages)
    target = root / "include" / "web_pages.h"
    # Não regravar sem necessidade evita recompilar à toa.
    if target.exists() and target.read_text(encoding="utf-8") == header:
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(header, encoding="utf-8")
    print("embed_web: include/web_pages.h atualizado")
    return True


try:
    Import("env")  # noqa: F821 - só existe quando o PlatformIO executa o script
except NameError:
    env = None

if env is not None:
    generate(env["PROJECT_DIR"])
elif __name__ == "__main__":
    generate(Path(__file__).resolve().parent.parent)
