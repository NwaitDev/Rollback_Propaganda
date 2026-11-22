#!/usr/bin/env python3
"""Plays one game of ./server with a ./bot process on some seats, and relays
the lines between them (see docs/protocol.md, section "Bots de test").

    scripts/play_with_bots.py --bot 0=random --bot 1=smart:12 --bot 2=random \\
        [--transcript game.jsonl] -- [options of ./server]

Every message of the server goes to every bot; the answer to a question goes
from the bot of its seat to the server. The seats without --bot must be
played by the engine (--seat k=random... in the options of ./server).
Prints the game_over message (without its state) and exits with the exit
code of the server.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--bot", action="append", default=[], metavar="SEAT=STRATEGY[:GREED]",
                        help="a ./bot on this seat (strategy random or smart, greed 10 by default)")
    parser.add_argument("--transcript", type=Path, help="writes every line exchanged in this file")
    parser.add_argument("--server", default=str(PROJECT / "server"))
    parser.add_argument("--bot-program", default=str(PROJECT / "bot"))
    parser.add_argument("server_args", nargs=argparse.REMAINDER,
                        help="options of ./server, after --")
    args = parser.parse_args(argv)
    server_args = args.server_args[1:] if args.server_args[:1] == ["--"] else args.server_args

    bots = {}
    for spec in args.bot:
        seat, _, who = spec.partition("=")
        strategy, _, greed = who.partition(":")
        command = [args.bot_program, "--seat", seat, "--strategy", strategy]
        if greed:
            command += ["--greed", greed]
        bots[int(seat)] = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           text=True, cwd=PROJECT)

    server = subprocess.Popen([args.server, "--quiet", *server_args], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, text=True, cwd=PROJECT)
    transcript = open(args.transcript, "w") if args.transcript else None
    last = None
    try:
        for line in server.stdout:
            if transcript:
                transcript.write(line)
            message = json.loads(line)
            last = message
            for bot in bots.values():
                if bot.poll() is None:
                    bot.stdin.write(line)
                    bot.stdin.flush()
            if message["type"] == "ask":
                bot = bots.get(message["seat"])
                if bot is None:
                    sys.exit(f"no --bot for seat {message['seat']}")
                answer = bot.stdout.readline()
                if not answer:
                    sys.exit(f"the bot of seat {message['seat']} stopped")
                if transcript:
                    transcript.write(answer)
                server.stdin.write(answer)
                server.stdin.flush()
    finally:
        if transcript:
            transcript.close()
        for bot in bots.values():
            try:
                bot.stdin.close()
            except BrokenPipeError:
                pass
            bot.wait(timeout=30)
        server.stdin.close()
        server.wait(timeout=30)

    if last is not None:
        last.pop("state", None)
        print(json.dumps(last))
    return server.returncode


if __name__ == "__main__":
    sys.exit(main())
