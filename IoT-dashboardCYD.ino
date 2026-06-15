#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// =============================================
//   CONFIGURAÇÕES
const char* ssid      = "VINI_biel-2G";
const char* password  = "@19061#!vb2";
const char* serverURL = "http://192.168.15.143:5000/status";

const int INTERVALO_MS = 5000;
// =============================================

TFT_eSPI tft = TFT_eSPI();

// Cores RGB565
#define COR_FUNDO        0x0841
#define COR_HEADER       0x0229
#define COR_VERDE        0x07E0
#define COR_AMARELO      0xFFE0
#define COR_LARANJA      0xFD20
#define COR_VERMELHO     0xF800
#define COR_BRANCO       0xFFFF
#define COR_CINZA_CLARO  0xC618
#define COR_TEXTO_DIM    0x8410
#define COR_AZUL_CLARO   0x051F
#define COR_DESTAQUE     0x07FF
#define COR_ROXO         0x180E

// Dimensões reais após detectar orientação
int LARGURA = 320;
int ALTURA  = 240;

unsigned long ultimaAtualizacao = 0;

// =============================================
//   SETUP
// =============================================
void setup() {
  Serial.begin(115200);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();

  // Testa rotation 0 — no seu CYD isso deve resultar em paisagem
  // Se ainda aparecer retrato, troque o 0 por 2
  tft.setRotation(2);

  LARGURA = tft.width();
  ALTURA  = tft.height();

  Serial.print("Largura: "); Serial.println(LARGURA);
  Serial.print("Altura:  "); Serial.println(ALTURA);

  tft.fillScreen(COR_HEADER);
  tft.setTextColor(COR_BRANCO);
  tft.setTextSize(1);

  // Mostra as dimensões na tela para confirmar orientação
  tft.setCursor(4, 4);
  tft.print("Tela: ");
  tft.print(LARGURA);
  tft.print("x");
  tft.print(ALTURA);
  tft.print("px  rotation=0");

  tft.setCursor(4, 18);
  tft.setTextColor(COR_AMARELO);
  tft.print("Se retrato: abra Serial e veja valores");

  delay(2000);

  tft.fillScreen(COR_HEADER);
  tft.setTextColor(COR_BRANCO);
  tft.setTextSize(2);
  tft.setCursor(LARGURA / 2 - 66, ALTURA / 2 - 20);
  tft.print("ELETROPOSTO");
  tft.setTextSize(1);
  tft.setTextColor(COR_AMARELO);
  tft.setCursor(LARGURA / 2 - 55, ALTURA / 2 + 8);
  tft.print("Conectando ao WiFi...");

  conectarWiFi();
}

// =============================================
//   LOOP
// =============================================
void loop() {
  if (millis() - ultimaAtualizacao >= INTERVALO_MS || ultimaAtualizacao == 0) {
    buscarEDesenhar();
    ultimaAtualizacao = millis();
  }
}

// =============================================
//   WIFI
// =============================================
void conectarWiFi() {
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    tentativas++;
  }

  tft.fillScreen(COR_HEADER);
  tft.setTextSize(1);

  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(COR_VERDE);
    tft.setCursor(4, ALTURA / 2 - 6);
    tft.print("WiFi OK! IP: ");
    tft.print(WiFi.localIP());
    delay(1500);
  } else {
    tft.setTextColor(COR_VERMELHO);
    tft.setCursor(4, ALTURA / 2 - 6);
    tft.print("FALHA no WiFi! Verifique nome e senha.");
    delay(3000);
  }
}

// =============================================
//   BUSCAR DADOS
// =============================================
void buscarEDesenhar() {
  if (WiFi.status() != WL_CONNECTED) {
    desenharErro("WiFi desconectado");
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(4000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    desenharErro("Sem resposta do servidor");
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    desenharErro("Erro ao ler JSON");
    return;
  }

  float       potencia    = doc["potencia_total_kw"];
  int         numVeiculos = doc["veiculos_carregando"];
  float       temperatura = doc["temperatura_c"];
  float       energia     = doc["energia_entregue_hoje_kwh"];
  float       eficiencia  = doc["eficiencia_pct"];
  int         tempoMedio  = doc["tempo_medio_sessao_min"];
  const char* marca       = doc["veiculo_destaque"]["marca"];
  const char* modelo      = doc["veiculo_destaque"]["modelo"];
  float       potVeiculo  = doc["veiculo_destaque"]["potencia_kw"];
  const char* status      = doc["status_sistema"];

  desenharDashboard(potencia, numVeiculos, temperatura, energia,
                    eficiencia, tempoMedio, marca, modelo, potVeiculo, status);
}

// =============================================
//   DASHBOARD — layout 100% dentro de LARGURA x ALTURA
//   Calculado para 320x240 (paisagem)
//   Todos os valores Y ficam entre 0 e 239
//   Todos os valores X ficam entre 0 e 319
// =============================================
void desenharDashboard(float potencia, int numVeiculos, float temperatura,
                       float energia, float eficiencia, int tempoMedio,
                       const char* marca, const char* modelo,
                       float potVeiculo, const char* status) {

  tft.fillScreen(COR_FUNDO);

  // --- VARIÁVEIS DE LAYOUT ---
  // Usa LARGURA e ALTURA para ser adaptável a qualquer rotação
  int W = LARGURA;   // 320
  int H = ALTURA;    // 240

  int headerH = 20;                        // altura do cabeçalho
  int rodapeH = 12;                        // altura do rodapé
  int areaUtil = H - headerH - rodapeH;    // 208px disponíveis para cards
  int margemX = 2;
  int margemY = 2;
  int espaco  = 3;

  // Larguras das colunas (duas colunas iguais)
  int colW = (W - margemX * 2 - espaco) / 2;  // ~157px cada

  // Alturas das linhas
  int linha1H = 58;   // potência + veículos
  int linha2H = 58;   // temperatura + eficiência
  int linha3H = 50;   // veículo destaque (largura total)
  int linha4H = areaUtil - linha1H - linha2H - linha3H - espaco * 3; // restante ~39px

  // Posições Y de cada linha
  int y1 = headerH + margemY;
  int y2 = y1 + linha1H + espaco;
  int y3 = y2 + linha2H + espaco;
  int y4 = y3 + linha3H + espaco;
  int yRodape = H - rodapeH;

  // Posições X das colunas
  int x1 = margemX;
  int x2 = margemX + colW + espaco;

  // -------------------------------------------------------
  // CABEÇALHO  y=0 até y=19
  // -------------------------------------------------------
  tft.fillRect(0, 0, W, headerH, COR_HEADER);

  tft.setTextSize(1);
  tft.setTextColor(COR_BRANCO);
  tft.setCursor(5, 3);
  tft.print("ELETROPOSTO");

  tft.setCursor(5, 12);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.print("Status:");
  tft.setTextColor(String(status) == "OPERACIONAL" ? COR_VERDE : COR_VERMELHO);
  tft.print(" ");
  tft.print(status);

  tft.setCursor(W - 72, 3);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.print("WiFi:");
  tft.setTextColor(COR_VERDE);
  tft.print(" ON");

  tft.setCursor(W - 72, 12);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.print("Up:");
  tft.setTextColor(COR_CINZA_CLARO);
  tft.print(millis() / 1000);
  tft.print("s");

  // -------------------------------------------------------
  // LINHA 1 — Potência (esq) + Veículos (dir)
  // -------------------------------------------------------

  // Card Potência
  tft.fillRoundRect(x1, y1, colW, linha1H, 5, 0x0229);
  tft.drawRoundRect(x1, y1, colW, linha1H, 5, COR_DESTAQUE);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y1 + 5);
  tft.print("POTENCIA TOTAL");
  tft.setTextSize(2);
  tft.setTextColor(COR_DESTAQUE);
  tft.setCursor(x1 + 5, y1 + 18);
  tft.print(potencia, 1);
  tft.setTextSize(1);
  tft.print(" kW");
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y1 + linha1H - 10);
  tft.print("Carregamento ativo");

  // Card Veículos
  tft.fillRoundRect(x2, y1, colW, linha1H, 5, 0x0229);
  tft.drawRoundRect(x2, y1, colW, linha1H, 5, COR_AMARELO);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x2 + 5, y1 + 5);
  tft.print("VEICULOS CARREGANDO");
  tft.setTextSize(3);
  tft.setTextColor(COR_AMARELO);
  tft.setCursor(x2 + colW / 2 - 12, y1 + 16);
  tft.print(numVeiculos);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x2 + 5, y1 + linha1H - 10);
  tft.print("Conectados agora");

  // -------------------------------------------------------
  // LINHA 2 — Temperatura (esq) + Eficiência (dir)
  // -------------------------------------------------------
  uint16_t corTemp = COR_VERDE;
  if (temperatura > 45.0) corTemp = COR_LARANJA;
  if (temperatura > 52.0) corTemp = COR_VERMELHO;

  // Card Temperatura
  tft.fillRoundRect(x1, y2, colW, linha2H, 5, 0x1082);
  tft.drawRoundRect(x1, y2, colW, linha2H, 5, corTemp);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y2 + 5);
  tft.print("TEMPERATURA");
  tft.setTextSize(2);
  tft.setTextColor(corTemp);
  tft.setCursor(x1 + 5, y2 + 18);
  tft.print(temperatura, 1);
  tft.setTextSize(1);
  tft.print(" C");
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y2 + linha2H - 10);
  if (temperatura < 45.0)      tft.print("Normal");
  else if (temperatura < 52.0) tft.print("Atencao: elevada");
  else                          tft.print("ALERTA: critica!");

  // Card Eficiência
  tft.fillRoundRect(x2, y2, colW, linha2H, 5, 0x1082);
  tft.drawRoundRect(x2, y2, colW, linha2H, 5, COR_VERDE);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x2 + 5, y2 + 5);
  tft.print("EFICIENCIA");
  tft.setTextSize(2);
  tft.setTextColor(COR_VERDE);
  tft.setCursor(x2 + 5, y2 + 18);
  tft.print(eficiencia, 1);
  tft.setTextSize(1);
  tft.print(" %");
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x2 + 5, y2 + linha2H - 10);
  tft.print("Rendimento atual");

  // -------------------------------------------------------
  // LINHA 3 — Veículo destaque (largura total)
  // -------------------------------------------------------
  int cardLargTotal = W - margemX * 2;
  tft.fillRoundRect(x1, y3, cardLargTotal, linha3H, 5, COR_ROXO);
  tft.drawRoundRect(x1, y3, cardLargTotal, linha3H, 5, COR_CINZA_CLARO);

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y3 + 5);
  tft.print("VEICULO NO CONECTOR 1");

  tft.setTextColor(COR_BRANCO);
  tft.setCursor(x1 + 5, y3 + 17);
  tft.print(marca);
  tft.print(" ");
  tft.print(modelo);

  tft.setTextColor(COR_AZUL_CLARO);
  tft.setCursor(x1 + 5, y3 + 29);
  tft.print("Potencia: ");
  tft.print(potVeiculo, 1);
  tft.print(" kW");

  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y3 + 39);
  tft.print("Sessao ativa");

  // Coluna direita do card veículo
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(W / 2 + 10, y3 + 17);
  tft.print("Tempo medio sessao:");
  tft.setTextColor(COR_AMARELO);
  tft.setCursor(W / 2 + 10, y3 + 29);
  tft.print(tempoMedio);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.print(" minutos");

  // -------------------------------------------------------
  // LINHA 4 — Energia entregue hoje (largura total)
  // -------------------------------------------------------
  tft.drawRoundRect(x1, y4, cardLargTotal, linha4H, 5, COR_LARANJA);

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(x1 + 5, y4 + 5);
  tft.print("ENERGIA ENTREGUE HOJE");

  tft.setTextColor(COR_LARANJA);
  tft.setCursor(x1 + 5, y4 + 17);
  tft.print(energia, 1);
  tft.print(" kWh");

  tft.setTextColor(COR_TEXTO_DIM);
  tft.setCursor(W / 2 + 10, y4 + 17);
  tft.print("Acumulado diario");
}

// =============================================
//   ERRO
// =============================================
void desenharErro(const char* mensagem) {
  tft.fillScreen(COR_FUNDO);
  tft.fillRect(0, 0, LARGURA, 20, COR_VERMELHO);
  tft.setTextColor(COR_BRANCO);
  tft.setTextSize(1);
  tft.setCursor(5, 6);
  tft.print("ERRO DE CONEXAO");
  tft.setCursor(5, ALTURA / 2 - 10);
  tft.setTextColor(COR_AMARELO);
  tft.print(mensagem);
  tft.setCursor(5, ALTURA / 2 + 5);
  tft.setTextColor(COR_TEXTO_DIM);
  tft.print("Tentando novamente em 5s...");
}