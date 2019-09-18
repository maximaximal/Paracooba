# Bash completion for paracuber.
# Run 'source parac.sh' or put this file in /etc/bash_completion.d/
# This script was taken (and then expanded and made compatible with ZSH) from
# https://github.com/rakhimov/scram/blob/43bb4fc23bfe2fbc1589f776f0b7bd4b9f55dd5a/scripts/scram.sh
# The GPLv3 license therefore applies to this script.
# The binary is completely independent of this script and the script is in no way required
# to use the tool. Therefore, GPLv3 should only apply to this script only and not to the tool itself.
# It coult also be replaced at a later stage.

# Enable bashcompinit if running under ZSH.
if [ ! -z "$ZSH_NAME" ]; then
    autoload bashcompinit
    bashcompinit
fi

########################################
# Searches for short and long options
# starting with "-" and prints them
#
# Globals:
#   None
# Arguments:
#   A line containing options
# Returns:
#   Short option
#   Long option
########################################
_parac_parse_options() {
  local short_option long_option w
  for w in $1; do
    case "${w}" in
      -?) [[ -n "${short_option}" ]] || short_option="${w}" ;;
      --*) [[ -n "${long_option}" ]] || long_option="${w}" ;;
    esac
  done

  [[ -n "${short_option}" ]] && printf "%s\n" "${short_option}"
  [[ -n "${long_option}" ]] && printf "%s\n" "${long_option}"
}

########################################
# Parses PARAC's Boost program options
# generated help description to find
# options
#
# Globals:
#   None
# Arguments:
#   None
# Returns:
#   Options
########################################
_parac_parse_help() {
  local line
  if [[ -f "$PWD/parac" ]]; then
    "$PWD/parac" --help | while read -r line; do
	[[ "${line}" == *-* ]] || continue
	_parac_parse_options "${line}"
    done
  else
    parac --help | while read -r line; do
	[[ "${line}" == *-* ]] || continue
	_parac_parse_options "${line}"
    done
  fi
}

########################################
# Bash completion function for PARAC
#
# Globals:
#   COMPREPLY
# Arguments:
#   None
# Returns:
#   Completion suggestions
########################################
_parac() {
  local cur prev type
  if ! type _init_completion >/dev/null; then
    _init_completion -n = || return
  else
    # manual initialization for older bash completion versions
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
  fi

  program="parac";
  if [[ -f "$PWD/parac" ]]; then
      program="$PWD/parac";
  fi

  case "${prev}" in
    --verbosity)
      COMPREPLY=($(compgen -W "0 1 2 3 4 5 6 7" -- "${cur}"))
      return
      ;;
    -*)
      type=$($program --help | \
        grep -Poi "^\s*(${prev}|\-\w \[ ${prev} \]|${prev} \[ --\S+ \])\s\K\w+")
      case "${type}" in
        path)
	  comptopt -o filenames 2>/dev/null
	  COMPREPLY=( $(compgen -f -- ${cur}) )
          return
          ;;
        bool)
          COMPREPLY=($(compgen -W "on off yes no true false 1 0" -- "${cur}"))
          return
          ;;
        double|int)
          # An argument is required.
          return
          ;;
      esac
  esac

  # Start parsing the help for option suggestions.
  if [[ "${cur}" == -* ]]; then
    COMPREPLY=($(compgen -W "$(_parac_parse_help)" -- "${cur}"))
    [[ -n "${COMPREPLY}" ]] && return
  fi

  # Default to input files.
  comptopt -o filenames 2>/dev/null
  COMPREPLY=( $(compgen -f -- ${cur}) )
}

complete -F _parac parac
