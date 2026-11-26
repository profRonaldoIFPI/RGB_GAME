"""Corrige uma falha da biblioteca ESPAsyncWebServer 3.12.1 antes de compilar.

O PROBLEMA: quando o navegador desconecta, a tarefa de rede apaga o ponteiro
`_client` SEM usar a trava. Ao mesmo tempo, o loop() do jogo pode estar usando
esse ponteiro para enviar o placar. No instante errado, o ESP32 reinicia.

A CORREÇÃO: usar a trava da própria biblioteca (ela é "recursiva") ao apagar e
ao consultar o ponteiro.

O PlatformIO roda este script antes de cada build. Ele altera a cópia baixada em
.pio/libdeps, e só uma vez. Se a versão da biblioteca mudar e o código não for
reconhecido, o build PARA com um erro: revise o remendo (ou veja se a biblioteca
já corrigiu a falha).
"""
from pathlib import Path

# arquivo -> [(código original, código corrigido)]
PATCHES = {
    "AsyncEventSource.cpp": [(
        """void AsyncEventSourceClient::_onDisconnect() {
  if (!_client) {
    return;
  }
  _client = nullptr;
  _server->_handleDisconnect(this);
}
""",
        """void AsyncEventSourceClient::_onDisconnect() {
  {
    // RGB GAME: espera quem estiver usando _client. A trava é solta antes de
    // _handleDisconnect() para manter a mesma ordem de travas do envio.
    asyncsrv::lock_guard_type lock(_lockmq);
    if (!_client) {
      return;
    }
    _client = nullptr;
  }
  _server->_handleDisconnect(this);
}
""")],
    "AsyncEventSource.h": [(
        """  bool connected() const {
    return _client && _client->connected();
  }
  uint32_t lastId() const {
""",
        """  bool connected() const {
    asyncsrv::lock_guard_type lock(_lockmq);  // RGB GAME: _client não muda durante a consulta
    return _client && _client->connected();
  }
  uint32_t lastId() const {
""")],
}


def patch_library(lib_dir):
    """Aplica o remendo. True se alterou algo; ValueError se não reconhecer o código."""
    changes = {}
    for name, pairs in PATCHES.items():
        path = Path(lib_dir) / "src" / name
        text = path.read_text(encoding="utf-8")
        original = text
        for old, new in pairs:
            if new in text:
                continue                      # já corrigido
            if text.count(old) != 1:
                raise ValueError(f"patch_asyncweb: código não reconhecido em {path}")
            text = text.replace(old, new)
        if text != original:
            changes[path] = text
    # Só grava depois de conferir todos os arquivos (nunca deixa pela metade).
    for path, text in changes.items():
        path.write_text(text, encoding="utf-8")
    return bool(changes)


try:
    Import("env")  # noqa: F821 - só existe quando o PlatformIO executa o script
except NameError:
    env = None

if env is not None:
    library = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "ESPAsyncWebServer"
    if not library.is_dir():
        raise RuntimeError("patch_asyncweb: biblioteca ESPAsyncWebServer não encontrada; compile de novo")
    if patch_library(library):
        print("patch_asyncweb: ESPAsyncWebServer corrigida")
