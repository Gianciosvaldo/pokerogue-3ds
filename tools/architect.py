import argparse
import os
import sys
import google.generativeai as genai

def main():
    parser = argparse.ArgumentParser(description="Zero-Shot Architect for Old 3DS Port")
    parser.add_argument("--ts", required=True, help="Path to upstream TypeScript file")
    parser.add_argument("--out", required=True, help="Path to output Markdown blueprint")
    parser.add_argument("--parity", default="docs/PARITY-TASKS.md", help="Path to PARITY-TASKS.md")
    args = parser.parse_args()

    # Authenticate statelessly
    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        sys.exit("Error: GEMINI_API_KEY environment variable not set. Run: export GEMINI_API_KEY='your_key'")
    
    genai.configure(api_key=api_key)

    # Ingest context
    try:
        with open(args.parity, "r") as f:
            parity_content = f.read()
        with open(args.ts, "r") as f:
            ts_content = f.read()
    except FileNotFoundError as e:
        sys.exit(f"I/O Error: {e}")

    # The Cold Auditor System Prompt
    system_instruction = """
    You are a strict software architecture auditor. Your task is to translate upstream TypeScript logic into a native C blueprint for the Old 3DS hardware.
    
    CRITICAL CONSTRAINTS:
    1. C99 Standard only.
    2. Zero dynamic allocation (no malloc/free).
    3. The execution must fit within a 1.51 ms median turn budget.
    4. Minimize struct padding to conserve memory.
    
    OUTPUT FORMAT REQUIREMENTS:
    1. A step-by-step logic flowchart mapping the state changes.
    2. Strict C struct definitions required to manage this state.
    3. DO NOT output any .c implementation files or executable logic.
    """

    prompt = f"""
    PROJECT PARITY RULES:
    {parity_content}
    
    UPSTREAM TYPESCRIPT TO TRANSLATE:
    {ts_content}
    """

    print(f"Auditing {args.ts}...")
    
    # gemini-1.5-flash is ideal here: fast, massive context window, and free-tier accessible
    model = genai.GenerativeModel(
        model_name='gemini-1.5-flash',
        system_instruction=system_instruction
    )
    
    # Zero-shot generation
    response = model.generate_content(prompt)

    # Write deterministic blueprint to disk
    with open(args.out, "w") as f:
        f.write(response.text)
    
    print(f"Strict blueprint generated at {args.out}")

if __name__ == "__main__":
    main()
