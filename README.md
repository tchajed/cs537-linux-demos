# Linux demos

## pipes.c

What happens if you don't close the unused ends of a pipe, as `pipe(2)` and
`pipe(7)` recommend? We can best test this by writing our own C code on both
ends of the pipe. The GNU utilities are a little too robust to illustrate the
behavior.

Following the man page, if we close this works:

```sh
make; ./pipes
```

Compare this to if we don't close the write end in the child (the consumer):

```sh
make; timeout 5 ./pipes --no-close-unused
```

`pipes` will now block forever (the `timeout 5` will stop it after 5 seconds).
