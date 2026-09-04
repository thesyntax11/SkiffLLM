_skiffllm_complete() {
    local cur prev
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    local subcommands="run model git session skill chat-template openai config server"
    local model_actions="list info install remove verify"
    local git_actions="diff review explain commit log status"
    local session_actions="list show use remove"
    local skill_actions="list show call enable disable"
    local chat_actions="list detect info"
    local openai_flags="--base-url --base --stream --json --no-json --temp --max-tokens --model --api-key"
    local config_actions="path show init help"
    local server_actions="health help"
    local options="--help --version --show-config --list-models --model-info --doctor --network --smoke --tokenize --warmup --code --serve --host --port --api-key --benchmark --model --model-dir --config --history --session --profile --system --stop --attach --file --export --chat-template --output --prompt --prompt-file --project --summarize --remember --forget --stdin --json --non-interactive --ctx --batch --ubatch --reserve-ctx --n-keep --threads --gpu-layers --mmap --no-mmap --mlock --flash-attn --no-flash-attn --numa --kv-offload --no-kv-offload --temp --top-p --top-k --min-p --typical --repeat-penalty --repeat-last-n --n-predict --seed --reset-history --no-save --auto-trim --no-auto-trim --color --no-color --no-banner --verbose --debug --context-bar --no-context-bar --backend-info --skills --no-skills --enable-skill --disable-skill"

    if [[ "${COMP_CWORD}" -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "${subcommands} ${options}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "model" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${model_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "git" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${git_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "session" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${session_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "skill" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${skill_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "chat-template" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${chat_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "config" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${config_actions}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "openai" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${openai_flags}" -- "${cur}") )
        return 0
    fi
    if [[ "${COMP_WORDS[1]}" == "server" && "${COMP_CWORD}" -eq 2 ]]; then
        COMPREPLY=( $(compgen -W "${server_actions}" -- "${cur}") )
        return 0
    fi

    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "${options}" -- "${cur}") )
        return 0
    fi

    case "${prev}" in
        --model|--model-dir|--config|--history|--output|--prompt-file|--attach|--file|--export|--project|--summarize)
            COMPREPLY=( $(compgen -f -- "${cur}") )
            return 0
            ;;
        --host)
            COMPREPLY=( $(compgen -W "127.0.0.1 0.0.0.0 ::1" -- "${cur}") )
            return 0
            ;;
        --port|--benchmark)
            COMPREPLY=( $(compgen -W "8080 8081 9090 3 5 10" -- "${cur}") )
            return 0
            ;;
        --api-key)
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

complete -F _skiffllm_complete skiffllm
