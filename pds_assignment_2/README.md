## Matrix Operator RPC Service (Assignment 2)

This project implements a remote matrix operator using ONC/Sun RPC. The server exposes procedures for matrix addition, multiplication, transpose, and inverse; the client interacts with the user and invokes the remote procedures.

### Prerequisites

- GCC or Clang with C11 support
- ONC RPC development headers (`libtirpc-dev` on most modern Linux distros)
- `rpcbind` service running locally (start with `sudo systemctl start rpcbind`)
- `make`, `pkg-config`, `bash`

> If compilation fails with `rpc/rpc.h: No such file or directory`, install the RPC development package. Example (Debian/Ubuntu): `sudo apt install libtirpc-dev`

### Build

```bash
cd pds_assignment_2
make -f Makefile.matrixOp
```

This produces two executables in the same directory:

- `matrixOp_server` – RPC server exposing matrix operations
- `matrixOp_client` – interactive client application

### Running

1. Ensure `rpcbind` is running.
2. From one terminal:
   ```bash
   ./matrixOp_server
   ```
3. From another terminal (can be another machine that can reach the server):
   ```bash
   ./matrixOp_client <server-hostname>
   ```
   Follow the on-screen menu to provide matrices and choose operations. Multiple clients can run concurrently.

### Sample Test Script

A non-interactive demonstration is provided under `tests/run_sample.sh`. After building the binaries:

```bash
chmod +x tests/run_sample.sh
./tests/run_sample.sh
```

The script launches the server, executes a series of operations via the client, and stores the captured transcript in a temporary file.

### Notes

- Input matrices are limited to 400 elements as defined in `matrixOp.x`.
- Matrix inverse uses Gauss–Jordan elimination with partial pivoting; singular or ill-conditioned matrices will return an error.
- This repository already contains the `rpcgen` outputs with the required modifications. Re-running `rpcgen` on `matrixOp.x` will overwrite those changes.
