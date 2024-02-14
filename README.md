# Linux demos

## pipes.c

What happens if you don't close the unused ends of a pipe, as `pipe(2)` and
`pipe(7)` recommend? We can best test this by writing our own C code on both
ends of the pipe. The GNU utilities are a little too robust to illustrate the
behavior.
