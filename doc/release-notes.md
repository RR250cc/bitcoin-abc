# Bitcoin ABC 0.26.7 Release Notes

Bitcoin ABC version 0.26.7 is now available from:

  <https://download.bitcoinabc.org/0.26.7/>

This release includes the following features and fixes:
 - `getblockchaininfo` now returns a new `time` field, that provides the chain
   tip time.
 - Add a `-daemonwait` option to `bitcoind` to wait for initialization to complete
   before putting the process in the background. This allows the user or parent
   process to more easily know whether the daemon started successfully by observing
   the program’s output or exit code.
