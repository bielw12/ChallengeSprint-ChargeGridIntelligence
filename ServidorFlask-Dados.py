from flask import Flask, jsonify
import random
import time

app = Flask(__name__)

veiculos = [
    {"marca": "Tesla", "modelo": "Model 3", "potencia": 50.0},
    {"marca": "BYD", "modelo": "Dolphin", "potencia": 30.0},
    {"marca": "Chevrolet", "modelo": "Bolt EV", "potencia": 25.0},
]

@app.route('/status')
def status():
    total_veiculos = random.randint(1, len(veiculos))
    carregando = veiculos[:total_veiculos]
    potencia_total = sum(v["potencia"] for v in carregando)
    temperatura = round(random.uniform(28.0, 55.0), 1)
    energia_hoje = round(random.uniform(50.0, 300.0), 1)
    eficiencia = round(random.uniform(92.0, 99.0), 1)
    tempo_medio = random.randint(20, 90)

    return jsonify({
        "potencia_total_kw": round(potencia_total, 1),
        "veiculos_carregando": total_veiculos,
        "temperatura_c": temperatura,
        "energia_entregue_hoje_kwh": energia_hoje,
        "eficiencia_pct": eficiencia,
        "tempo_medio_sessao_min": tempo_medio,
        "veiculo_destaque": {
            "marca": carregando[0]["marca"],
            "modelo": carregando[0]["modelo"],
            "potencia_kw": carregando[0]["potencia"]
        },
        "status_sistema": "OPERACIONAL"
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
