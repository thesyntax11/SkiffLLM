_skifflm_complete() {
    local cur prev
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    local options="--help --version --show-config --list-models --model-info --doctor --smoke --tokenize --warmup --model --model-dir --config --history --session --profile --system --stop --attach --file --export --chat-template --output --prompt --prompt-file --stdin --json --non-interactive --ctx --batch --ubatch --reserve-ctx --n-keep --threads --gpu-layers --mmap --no-mmap --mlock --flash-attn --no-flash-attn --numa --kv-offload --no-kv-offload --temp --top-p --top-k --min-p --typical --repeat-penalty --repeat-last-n --n-predict --seed --reset-history --no-save --auto-trim --no-auto-trim --color --no-color --no-banner --verbose --debug"

    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "${options}" -- "${cur}") )
        return 0
    fi

    case "${prev}" in
        --model|--model-dir|--config|--history|--output|--prompt-file|--attach|--file|--export)
            COMPREPLY=( $(compgen -f -- "${cur}") )
            return 0
            ;;
        --profile)
            COMPREPLY=( $(compgen -W "balanced fast creative code precise" -- "${cur}") )
            return 0
            ;;
        --chat-template)
            COMPREPLY=( $(compgen -W "chatml llama2 mistral gemma phi qwen default" -- "${cur}") )
            return 0
            ;;
        --session)
            COMPREPLY=( $(compgen -W "default main writing coding help" -- "${cur}") )
            return 0
            ;;
    esac

    COMPREPLY=( $(compgen -f -- "${cur}") )
}

complete -F _skifflm_complete skifflm
