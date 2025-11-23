#!/usr/bin/env python3
import subprocess
import os
import sys

# Configurações
SOURCE_FILE = "dining_hall_logged.c"
BINARY_NAME = "./dining_hall_logged"
OUTPUT_DIR = "trace_logs"
SCENARIOS = [2, 3, 10] # Cenários representativos para o apêndice

def setup():
    print("🛠️  Preparando ambiente...")
    # Cria diretório
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
    
    # Compila
    cmd = ["gcc", "-Wall", "-pthread", "-O2", "-o", BINARY_NAME, SOURCE_FILE]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("❌ Erro de compilação.")
        sys.exit(1)

def generate_traces():
    print(f"📂 Gerando logs em ./{OUTPUT_DIR}/ ...")
    
    for n in SCENARIOS:
        log_filename = os.path.join(OUTPUT_DIR, f"trace_{n}_students.txt")
        print(f"   ➡️  Rodando cenário: {n} estudantes -> {log_filename}")
        
        # Define a variável de ambiente apenas para este subprocesso
        env_vars = os.environ.copy()
        env_vars["DINING_LOG_FILE"] = log_filename
        
        try:
            subprocess.run(
                [BINARY_NAME, str(n)],
                env=env_vars,
                timeout=10, # Timeout de segurança
                check=True
            )
        except subprocess.TimeoutExpired:
            print(f"      ⚠️  Timeout no cenário {n} (Deadlock?)")
        except subprocess.CalledProcessError:
            print(f"      ❌ Erro de execução no cenário {n}")

    print("\n✅ Geração de logs concluída com sucesso!")

if __name__ == "__main__":
    setup()
    generate_traces()
