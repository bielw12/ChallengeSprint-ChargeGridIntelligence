# ⚡ Eletroposto Dashboard + ChargeGrid AI

Sistema completo de monitoramento embarcado e suporte técnico inteligente para eletropostos.  
Desenvolvido para a disciplina de Computação em Nuvem e Sistemas Embarcados — Turma 1CCP.

---

## Visão Geral 

Este projeto é composto por dois módulos independentes, mas complementares:

| Módulo | Tecnologia | Status |
|---|---|---|
| **Dashboard Embarcado** | ESP32 CYD + C++ + Python Flask | ✅ Funcionando |
| **ChargeGrid AI** (chatbot de suporte) | FastAPI + RAG + LLaMA 3.3 | 🔧 Em desenvolvimento separado |

O **Dashboard Embarcado** exibe em tempo real informações operacionais do eletroposto diretamente em uma tela física acoplada à placa ESP32 CYD (Cheap Yellow Display). Os dados são servidos por um servidor local em Python (Flask) e consumidos pela placa via WiFi usando o protocolo HTTP.

O **ChargeGrid AI** é um assistente técnico inteligente baseado em RAG (Retrieval Augmented Generation) que permite a operadores, síndicos e técnicos obterem respostas rápidas sobre o carregador GoodWe HCA-G2 sem consultar manuais PDF.

---

## Parte 1 — Dashboard Embarcado

### O que é

Um painel físico de monitoramento exibido em uma tela TFT de 2,8 polegadas acoplada a uma placa ESP32. A placa se conecta à rede WiFi local, consulta uma API REST rodando no computador do operador e renderiza o dashboard diretamente no display — sem navegador, sem HTML, desenhado pixel a pixel em C++.

### Arquitetura do sistema

```
┌─────────────────────────────────────────┐
│           Sua Máquina (PC)              │
│                                         │
│  servidor_eletroposto.py                │
│  ┌──────────────┐   ┌────────────────┐  │
│  │ Script Python│──▶│ Servidor Flask │  │
│  │ Gera dados   │   │ Porta 5000     │  │
│  └──────────────┘   └───────┬────────┘  │
│                             │           │
│                      API REST /status   │
│                      Resposta JSON      │
└─────────────────────────────────────────┘
                             │
                    Rede WiFi 2.4 GHz
                    HTTP GET /status
                             │
┌─────────────────────────────────────────┐
│           ESP32 CYD (Placa)             │
│                                         │
│  ┌─────────────┐  ┌──────────────────┐  │
│  │  WiFi.h     │  │  HTTPClient.h    │  │
│  │ Conecta rede│  │ Faz requisição   │  │
│  └─────────────┘  └──────────────────┘  │
│  ┌─────────────┐  ┌──────────────────┐  │
│  │ArduinoJson.h│  │  TFT_eSPI.h      │  │
│  │ Lê o JSON   │  │ Desenha na tela  │  │
│  └─────────────┘  └──────────────────┘  │
│                          │               │
│                   Display ILI9341        │
│                   240 × 320 px          │
└─────────────────────────────────────────┘
```

### Informações exibidas no dashboard

- Potência total em kW (carregamento ativo)
- Quantidade de veículos carregando no momento
- Temperatura do sistema (com alerta visual por cor)
- Eficiência do sistema em %
- Veículo em destaque no conector 1 (marca, modelo, potência individual)
- Energia total entregue no dia em kWh
- Tempo médio de sessão em minutos
- Status do sistema (OPERACIONAL / FALHA)
- Uptime da placa em segundos
- Indicador de conexão WiFi

### Stack tecnológica

| Camada | Tecnologia |
|---|---|
| Linguagem do firmware | C++ (Arduino Framework) |
| Servidor de dados | Python 3 + Flask |
| Protocolo de comunicação | HTTP REST (polling a cada 5s) |
| Formato de dados | JSON |
| Display | ILI9341 — 2.8", 240×320 px |
| Biblioteca de display | TFT_eSPI (Bodmer) |
| Parsing JSON | ArduinoJson (Benoit Blanchon) |
| Plataforma | ESP32 Dev Module |
| IDE | Arduino IDE 2.x |

### Servidor Flask — trecho de código

O servidor Python simula dados de um eletroposto real e os expõe via endpoint REST:

```python
from flask import Flask, jsonify
import random

app = Flask(__name__)

veiculos = [
    {"marca": "Tesla",     "modelo": "Model 3",  "potencia": 50.0},
    {"marca": "BYD",       "modelo": "Dolphin",  "potencia": 30.0},
    {"marca": "Chevrolet", "modelo": "Bolt EV",  "potencia": 25.0},
]

@app.route('/status')
def status():
    total = random.randint(1, len(veiculos))
    carregando = veiculos[:total]
    return jsonify({
        "potencia_total_kw":          round(sum(v["potencia"] for v in carregando), 1),
        "veiculos_carregando":        total,
        "temperatura_c":              round(random.uniform(28.0, 55.0), 1),
        "energia_entregue_hoje_kwh":  round(random.uniform(50.0, 300.0), 1),
        "eficiencia_pct":             round(random.uniform(92.0, 99.0), 1),
        "tempo_medio_sessao_min":     random.randint(20, 90),
        "veiculo_destaque":           carregando[0],
        "status_sistema":             "OPERACIONAL"
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

### Firmware C++ — trecho principal (loop de atualização)

A placa consulta a API a cada 5 segundos e redesenha o dashboard com os novos dados:

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

const char* ssid      = "SUA_REDE";
const char* password  = "SUA_SENHA";
const char* serverURL = "http://192.168.1.100:5000/status";
const int   INTERVALO_MS = 5000;

TFT_eSPI tft = TFT_eSPI();

void loop() {
  if (millis() - ultimaAtualizacao >= INTERVALO_MS) {
    buscarEDesenhar();
    ultimaAtualizacao = millis();
  }
}

void buscarEDesenhar() {
  HTTPClient http;
  http.begin(serverURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    float potencia    = doc["potencia_total_kw"];
    int   veiculos    = doc["veiculos_carregando"];
    float temperatura = doc["temperatura_c"];
    // ... lê os demais campos e desenha na tela
  }
  http.end();
}
```

### Configuração do display (TFT_eSPI — User_Setup.h)

O CYD usa o chip ILI9341 com pinos específicos. Configure o arquivo `User_Setup.h` da biblioteca:

```cpp
#define ILI9341_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define SPI_FREQUENCY  55000000
```

### Como rodar o dashboard

**Pré-requisitos:** Python 3, pip, Arduino IDE com suporte ao ESP32.

```bash
# 1. Instalar o Flask
pip install flask

# 2. Rodar o servidor de dados
python servidor_eletroposto.py
# Servidor disponível em http://0.0.0.0:5000

# 3. Descobrir o IP da sua máquina
ipconfig        # Windows
ifconfig        # Linux / Mac

# 4. Editar o firmware C++ com sua rede e IP, depois:
# Abrir o Arduino IDE → Upload para o ESP32 (segurar BOOT se travar)
```

Acesse `http://SEU_IP:5000/status` no navegador para verificar se o servidor responde antes de gravar o firmware.

### Estrutura de arquivos — Dashboard

```
eletroposto-dashboard/
├── servidor_eletroposto.py    # Servidor Flask com dados simulados
└── firmware/
    └── dashboard.ino          # Código C++ para o ESP32 CYD
```

---

## Parte 2 — ChargeGrid AI (Chatbot de Suporte Técnico)

> **Status:** Repositório independente — em desenvolvimento.  
> Este módulo complementa o dashboard com suporte técnico inteligente ao usuário final.

### O que é

O ChargeGrid AI é um assistente técnico especializado no carregador **GoodWe HCA-G2**, construído com a técnica de **RAG (Retrieval Augmented Generation)**. Ele responde perguntas técnicas em linguagem natural consultando uma base de conhecimento derivada do manual oficial — sem inventar informações, sem consultar PDF, em menos de 1 segundo.

### Problema que resolve

Os eletropostos GoodWe HCA-G2 carecem de mecanismos integrados para:

- Diagnóstico rápido de falhas por LED e conectividade
- Orientação sobre faturamento e custeio por sessão
- Suporte a configuração de Dynamic Load Control
- Consulta à tabela completa de registros Modbus TCP

O ChargeGrid AI resolve isso entregando conhecimento técnico acessível via API REST — para operadores, síndicos e técnicos.

### Pipeline RAG

```
Pergunta do Usuário
        │
        ▼
  [1] RETRIEVAL
  Busca Semântica
  FAISS + keyword hints
  → 8 contextos relevantes
        │
        ▼
  [2] AUGMENTATION
  Injeta contexto + system prompt
  Temperature: 0.1 (determinístico)
        │
        ▼
  [3] GENERATION
  Groq + llama-3.3-70b-versatile
  Latência P50: ~800ms
        │
        ▼
  [4] RESPONSE
  Resposta estruturada + fontes citadas
```

### Stack tecnológica

| Camada | Tecnologia | Detalhe |
|---|---|---|
| Backend | FastAPI (Python) | Assíncrono, docs OpenAPI automáticas |
| Embedding | HuggingFace `all-MiniLM-L6-v2` | 384 dimensões, <5ms por documento |
| Vector Store | FAISS (Meta) | Local, privado, recall de 98% |
| LLM | Groq + `llama-3.3-70b-versatile` | <1s de latência |
| Orquestração | LangChain | Abstração de provider e retrieval MMR |
| API | REST + JSON | Endpoint único `POST /chat` |

### Exemplo de uso da API

```bash
curl -X POST http://localhost:8000/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "Qual é o registro Modbus para ligar o carregamento?"}'
```

```json
{
  "response": "O registro Modbus para controlar o carregamento é o 10060 (Turn on/off charging). Use valor 2 para ligar e valor 1 para desligar...",
  "sources": [
    {"source": "modbus_reference.txt", "rank": 1},
    {"source": "carregamento.txt",     "rank": 2}
  ]
}
```

### Base de conhecimento

12 documentos técnicos derivados do **Manual Oficial GoodWe HCA-G2 V1.5** — ~19KB indexados.

| Arquivo | Conteúdo |
|---|---|
| `autenticacao.txt` | RFID, SolarGo, SEMS Portal, AUTO Start |
| `carregamento.txt` | Modos, parâmetros, potência, status |
| `comunicacao.txt` | Modbus TCP, RS485, Wi-Fi, Bluetooth |
| `eficiencia_energetica.txt` | PV Priority, Dynamic Load Control |
| `especificacoes_tecnicas.txt` | Modelos GW7K / GW11K / GW22K |
| `faturamento.txt` | Medidor MID, energy tracking, custo |
| `manutencao.txt` | Procedimentos, firmware, descarte |
| `monitoramento.txt` | LED, registros Modbus, alarmes |
| `seguranca.txt` | Proteções, RCBO 30mA, aterramento, IK10 |
| `troubleshooting_guide.txt` | 10+ falhas mapeadas com soluções |
| `modbus_reference.txt` | 100+ registros TCP com ganhos e tipos |
| `conectividade.txt` | Topologias, inversores, medidores |

### Parâmetros de configuração RAG

```
Chunking:
  Tamanho:  900 caracteres
  Overlap:  180 caracteres (20%)

Retrieval:
  Método:   Max Marginal Relevance (MMR)
  K final:  8 documentos
  Fetch K:  30 candidatos

LLM:
  Provider:     Groq
  Modelo:       llama-3.3-70b-versatile
  Temperature:  0.1
  Max tokens:   900
```

### Como rodar o ChargeGrid AI

```bash
# 1. Clonar o repositório
git clone <repo-url>
cd chargegrid-ai

# 2. Instalar dependências
pip install -r requirements.txt

# 3. Configurar a chave da API Groq
echo "GROQ_API_KEY=gsk_..." > .env

# 4. Gerar o vector store (apenas na primeira vez)
python create_vector_store.py

# 5. Iniciar o servidor
uvicorn app.main:app --reload

# Acesse a documentação interativa em:
# http://localhost:8000/docs
```

### Estrutura de arquivos — ChargeGrid AI

```
chargegrid-ai/
├── app/
│   ├── main.py                    # FastAPI — entry point
│   ├── routes/
│   │   └── chat.py                # Endpoint POST /chat
│   ├── services/
│   │   ├── embedding_service.py   # Criação de embeddings FAISS
│   │   ├── rag_service.py         # Busca semântica no vector store
│   │   └── ai_service.py          # Integração Groq / LLaMA
│   ├── models/
│   │   └── schemas.py             # Pydantic models (input / output)
│   ├── prompts/
│   │   └── system_prompt.txt      # Instruções de comportamento do AI
│   └── rag/
│       ├── docs/                  # Documentos TXT (base de conhecimento)
│       └── vector_store/          # Índice FAISS (gerado automaticamente)
├── create_vector_store.py
├── requirements.txt
└── .env                           # GROQ_API_KEY (nunca commitar)
```

### Métricas de performance

| Métrica | Target | Resultado |
|---|---|---|
| Latência P50 | < 1s | ~800ms |
| Latência P99 | < 3s | ~2s |
| Acurácia técnica | > 85% | Baseado nos docs oficiais |
| Cobertura de tópicos | > 95% | 12 documentos principais |
| Taxa de erro | < 5% | Validado com test suite |

---

## Relação entre os módulos

Os dois módulos são independentes, mas foram concebidos para operar juntos no contexto de um eletroposto real:

```
Operador / Técnico
       │
       ├──▶ Olha para a tela física do ESP32 CYD
       │    Vê: potência, veículos, temperatura, energia
       │    (Dashboard Embarcado — C++ + Flask)
       │
       └──▶ Pergunta no chatbot via app ou terminal
            "Por que o LED está vermelho?"
            "Como configuro o Dynamic Load Control?"
            (ChargeGrid AI — FastAPI + RAG + LLaMA)
```

O dashboard resolve o **monitoramento em tempo real**. O chatbot resolve o **suporte e diagnóstico técnico**. Juntos, cobrem os dois principais pontos de atrito operacional de um eletroposto.

---

## Requisitos gerais

| Requisito | Dashboard | ChargeGrid AI |
|---|---|---|
| Python 3.10+ | ✅ | ✅ |
| pip | ✅ | ✅ |
| Arduino IDE 2.x | ✅ | — |
| Placa ESP32 CYD 2.8" | ✅ | — |
| Chave de API Groq | — | ✅ |
| Rede WiFi 2.4 GHz | ✅ | — |

---

## Segurança e boas práticas

- Chave de API Groq armazenada em `.env` — nunca em código-fonte
- Vector store com indexação local — sem dependência de serviço externo
- Validação de input/output via Pydantic schemas (ChargeGrid AI)
- Comunicação HTTP local (Dashboard) — sem exposição à internet
- `.env` deve estar no `.gitignore`

---

## Modelos de carregador suportados (ChargeGrid AI)

| Modelo | Potência | Uso típico |
|---|---|---|
| GW7K-HCA-20 | 7 kW | Residencial / leve comercial |
| GW11K-HCA-20 | 11 kW | Comercial padrão |
| GW22K-HCA-20 | 22 kW | Comercial intensivo |

Protocolos: Modbus TCP · RS485 · Wi-Fi · Bluetooth  
Aplicações: SolarGo (Bluetooth) · SEMS Portal (Cloud)

---

Obrigado!
