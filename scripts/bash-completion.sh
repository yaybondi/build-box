_build_box_arg_complete() {
    local _build_box_cmd="${COMP_WORDS[0]} ${COMP_WORDS[1]}"
    local _previous_arg="${COMP_WORDS[$(($COMP_CWORD-1))]}"

    local _opt_arg=""

    case "$_previous_arg" in
        -*)
            _opt_arg=$(
                $_build_box_cmd --help | \
                    grep -E '^ -' | \
                    sed -E -e 's/^\s*/,/g' -e 's/ /,/' -e 's/  .*$//g' | \
                    grep -E -- ",$_previous_arg," | \
                    cut -d',' -f4
            )
            ;;
    esac

    case "$_opt_arg" in
        '<arch>')
            COMPREPLY=(
                $(
                    compgen -W "aarch64 armv4t armv6 armv7a i686 mipsel mips64el powerpc64le x86_64" \
                        -- ${COMP_WORDS[COMP_CWORD]}
                )
            )
            return
            ;;
        '<dir>')
            _cd
            return
            ;;
        '<fstype>')
            COMPREPLY=(
                $(
                    compgen -W "dev proc sys home" -- ${COMP_WORDS[COMP_CWORD]}
                )
            )
            return
            ;;
    esac

    local _word_counter=0
    local _varg_counter=0
    local _target_dir="$HOME/.bolt/targets"

    while [ "$_word_counter" -lt "${#COMP_WORDS[*]}" ]; do
        local _varg="${COMP_WORDS[$_word_counter]}"

        case "$_varg" in
            -t|--targets)
                case "${COMP_WORDS[$(($_word_counter+1))]}" in
                    =)
                        _target_dir="${COMP_WORDS[$(($_word_counter+2))]}"
                        _word_counter=$(($_word_counter+3))
                        ;;
                    *)
                        _target_dir="${COMP_WORDS[$(($_word_counter+1))]}"
                        _word_counter=$(($_word_counter+2))
                        ;;
                esac
                ;;
            -*)
                _opt_arg=$(
                    $_build_box_cmd --help | \
                        grep -E '^ -' | \
                        sed -E -e 's/^\s*/,/g' -e 's/ /,/' -e 's/  .*$//g' | \
                        grep -E -- ",$_varg," | \
                        cut -d',' -f4
                )

                if [ -n "$_opt_arg" ]; then
                    case "${COMP_WORDS[$(($_word_counter+1))]}" in
                        =)
                            _word_counter=$(($_word_counter+3))
                            ;;
                        *)
                            _word_counter=$(($_word_counter+2))
                            ;;
                    esac
                else
                    _word_counter=$(($_word_counter+1))
                fi
                ;;
            *)
                _word_counter=$(($_word_counter+1))
                _varg_counter=$(($_varg_counter+1))
                ;;
        esac
    done

    local _expected_arg=$(
        $_build_box_cmd --help | \
            grep -E '^Usage:' | \
            sed -E -e 's/^Usage:\s*//' -e 's/\[OPTIONS\]//' -e 's/\s+/ /g' | \
            cut -d' ' -f"$(($_varg_counter))"
    )

    case "$_expected_arg" in
        '<target-name>')
            COMPREPLY=(
                $(
                    compgen -W "$(
                        ${COMP_WORDS[0]} list --targets="$_target_dir" | \
                            cut -d' ' -f1
                    )" -- ${COMP_WORDS[COMP_CWORD]}
                )
            )
            ;;
        '<dir>')
            _cd
            ;;
    esac
}

_build_box_opt_complete() {
    local _build_box_cmd="${COMP_WORDS[0]} ${COMP_WORDS[1]}"

    COMPREPLY=(
        $(
            compgen -W "$(
                $_build_box_cmd --help | \
                    grep -E '^ -' | \
                    sed -E -e 's/^\s*//g' | \
                    cut -d' ' -f1 | \
                    sed 's/,/ /g'
            )" -- ${COMP_WORDS[COMP_CWORD]}
        )
    )
}

_build_box_complete() {
    local _valid_commands=$(
        ${COMP_WORDS[0]} --help | \
            grep -E '^  \w' | \
            sed -E 's/^\s*//g' | \
            cut -d' ' -f1
    )

    case "$COMP_CWORD" in
        1)
            COMPREPLY=(
                $(
                    compgen -W "$_valid_commands -h --help" \
                        -- ${COMP_WORDS[COMP_CWORD]}
                )
            )
            ;;
        *)
            _valid_commands=$(echo $_valid_commands | sed 's/ /|/g')

            eval "case \"${COMP_WORDS[1]}\" in
                $_valid_commands)
                    case \"${COMP_WORDS[COMP_CWORD]}\" in
                        -*)
                            _build_box_opt_complete
                            ;;
                        *)
                            _build_box_arg_complete
                            ;;
                    esac
                    ;;
            esac"
            ;;
    esac
}

complete -F _build_box_complete build-box
