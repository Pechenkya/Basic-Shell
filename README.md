# Basic shell

**Language:** C

## Short descriprion
This shell is part of course "Introduction to OS: Part 1" by Codio. 
It's a wrapper for UNIX file system, written with C language.

## Build-in commands
List of build-in commands that can be executed by default:

1. **exit** -- ends shell execution (no parameters)
2. **cd** -- changes current direction (exactly 1 parameter)
3. **path** -- set paths for executing other commands (0 or more parameters)

## Special possibilities

1. **Stream redirection** -- use symbol > to redirect output to file
2. **Parallel commands** -- use symbol & to call commands simultaneously

## Shell execution

Use this commands to start shell:

### Interactive mode

```
gcc -o wish wish.c
./wish
```

### Batch mode (from file batch.txt)

```
gcc -o wish wish.c
./wish batch.txt
```

#### REMARKS

1. Build-In commands cannot be run in parallel mode!

#### ISSUES
1. Some syntax and command execution problems sometimes (fix needed)
2. Code refactoring needed (add some space optimization and clarification)
