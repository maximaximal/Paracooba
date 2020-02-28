TCP Communication {#tcpcommunication}
=====================================

TCP Communication between nodes follows the following structure. Two different concepts of TCP
communication exist in the program: communication for jobs (paths, CNF formulas, results, ...),
the other one is for long-range meta network links (status updates, what would otherwise be
sent over UDP, *not yet implemented*).

Job Communication
-----------------

| Byte | Name              |
|------|-------------------|
| 0-4  | Originator ID     |
| 4-5  | Transmission Type |

The transmission type maps into these cases:

### DIMACS

| Byte | Name                                                |
|------|-----------------------------------------------------|
| 0-n  | Filename (Terminated with \0)                       |
| n-m  | File Contents (Terminated with end of transmission) |

### JobDescription

| Byte | Name                                                               |
|------|--------------------------------------------------------------------|
| 0-4  | Size of JobDescription to receive.                                 |
| 4-m  | paracooba::messages::JobDescription (binary serialised via Cereal) |

Meta Network Link
-----------------

*not yet implemented*
