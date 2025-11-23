#!/usr/bin/env python3
import subprocess
import sys
import time
import shutil

# --- Configurações do Teste ---
BINARY_NAME = "./dining_hall"
TIMEOUT_SECONDS = 5
NUM_RUNS = 30
SCENARIOS = [
    {"users": 2,  "label": "Par (Minimal Check)"},
    {"users": 3,  "label": "Ímpar (Edge Case - Sobra 1?)"},
    {"users": 10, "label": "Grupo Pequeno (Concorrência Padrão)"},
    {"users": 50, "label": "Carga Alta (Stress Test)"}
]

# --- Cores para Terminal (ANSI) ---
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def print_status(msg, color=Colors.OKBLUE):
    print(f"{color}{msg}{Colors.ENDC}")

def check_compilation():
    """Limpa e recompila o projeto para garantir binário fresco."""
    print_status(f"🔨 [SETUP] Compilando {BINARY_NAME}...", Colors.HEADER)
    
    # Verifica se make está instalado
    if not shutil.which("make"):
        print_status("❌ Erro: 'make' não encontrado no PATH.", Colors.FAIL)
        sys.exit(1)

    try:
        # Limpa build anterior
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # Compila
        result = subprocess.run(["make"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        if result.returncode != 0:
            print_status("❌ Erro de Compilação:", Colors.FAIL)
            print(result.stderr)
            sys.exit(1)
        
        print_status("✅ Compilação bem-sucedida.\n", Colors.OKGREEN)
        
    except Exception as e:
        print_status(f"❌ Exceção durante compilação: {e}", Colors.FAIL)
        sys.exit(1)

def run_stress_test():
    """Executa a bateria de testes."""
    print_status(f"🚀 [START] Iniciando bateria de testes de stress ({NUM_RUNS} runs/cenário)", Colors.BOLD)
    print(f"⏱️  Timeout definido: {TIMEOUT_SECONDS}s por execução\n")

    summary = []

    for scenario in SCENARIOS:
        users = scenario["users"]
        label = scenario["label"]
        
        print(f"{Colors.HEADER}Teste: {users} Estudantes - [{label}]{Colors.ENDC}")
        sys.stdout.write("Progresso: ")
        sys.stdout.flush()

        success_count = 0
        fail_count = 0
        deadlocks = 0
        avg_time = 0
        
        for i in range(NUM_RUNS):
            start_time = time.time()
            try:
                # Executa o binário e espera finalizar
                proc = subprocess.run(
                    [BINARY_NAME, str(users)], 
                    timeout=TIMEOUT_SECONDS,
                    stdout=subprocess.DEVNULL, # Silencia output do C para não poluir
                    stderr=subprocess.PIPE
                )
                
                if proc.returncode == 0:
                    sys.stdout.write(f"{Colors.OKGREEN}.{Colors.ENDC}") # Ponto verde = Sucesso
                    success_count += 1
                    avg_time += (time.time() - start_time)
                else:
                    sys.stdout.write(f"{Colors.FAIL}E{Colors.ENDC}") # E = Erro de Runtime (segfault, etc)
                    fail_count += 1

            except subprocess.TimeoutExpired:
                sys.stdout.write(f"{Colors.FAIL}D{Colors.ENDC}") # D = Deadlock (Timeout)
                deadlocks += 1
                # O processo é morto automaticamente pelo python após o timeout exception, 
                # mas para garantir limpeza em casos extremos, o subprocess.run cuida disso no Python 3.7+
            
            sys.stdout.flush()

        # Calcula média
        final_avg = (avg_time / success_count) if success_count > 0 else 0
        
        # Armazena estatísticas
        summary.append({
            "users": users,
            "success": success_count,
            "fails": fail_count,
            "deadlocks": deadlocks,
            "avg_time": final_avg
        })
        print("\n") # Quebra linha após os pontos

    return summary

def print_report(summary):
    """Gera a tabela final de resultados."""
    print_status("\n📊 [RELATÓRIO FINAL DE QA]\n", Colors.BOLD)
    
    # Cabeçalho da tabela
    print(f"{'Cenário':<20} | {'Sucesso':<10} | {'Deadlocks':<10} | {'Falhas':<10} | {'Tempo Médio (s)':<15}")
    print("-" * 80)
    
    all_passed = True
    
    for item in summary:
        scenario_str = f"{item['users']} Estudantes"
        
        # Formatação condicional
        success_str = f"{Colors.OKGREEN}{item['success']}{Colors.ENDC}" if item['success'] == NUM_RUNS else f"{Colors.WARNING}{item['success']}{Colors.ENDC}"
        deadlock_str = f"{Colors.FAIL}{item['deadlocks']}{Colors.ENDC}" if item['deadlocks'] > 0 else f"{Colors.OKGREEN}0{Colors.ENDC}"
        
        if item['deadlocks'] > 0 or item['fails'] > 0:
            all_passed = False

        print(f"{scenario_str:<20} | {success_str:<19} | {deadlock_str:<19} | {item['fails']:<10} | {item['avg_time']:.4f}")

    print("-" * 80)
    
    if all_passed:
        print_status("\n🏆 RESULTADO: APROVADO. O código é robusto e livre de deadlocks nos cenários testados.", Colors.OKGREEN)
        sys.exit(0)
    else:
        print_status("\n💀 RESULTADO: REPROVADO. Foram detectados problemas de estabilidade.", Colors.FAIL)
        sys.exit(1)

if __name__ == "__main__":
    check_compilation()
    results = run_stress_test()
    print_report(results)
