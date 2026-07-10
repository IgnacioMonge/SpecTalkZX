#!/usr/bin/env python3
"""Small regression check for BPE backup/restore transactions."""

import hashlib
import os
import shutil
import tempfile

import bpe_build as bpe


def tree_hashes(root):
    result = {}
    for rel_path, _ in bpe.BACKUP_FILES:
        with open(os.path.join(root, rel_path), "rb") as source:
            result[rel_path] = hashlib.sha256(source.read()).digest()
    return result


def make_sources(root):
    for rel_path, name in bpe.BACKUP_FILES:
        path = os.path.join(root, rel_path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        data = bytes(bpe.EXPECTED_DAT_SIZE) if name == "SPECTALK.DAT" else name.encode()
        with open(path, "wb") as output:
            output.write(data)


def main():
    saved = bpe.ROOT, bpe.BUILD_DIR, bpe.BPE_STAMP, bpe.BACKUP_DIR
    try:
        with tempfile.TemporaryDirectory() as root:
            bpe.ROOT = root
            bpe.BUILD_DIR = os.path.join(root, "build")
            bpe.BPE_STAMP = os.path.join(bpe.BUILD_DIR, ".bpe.stamp")
            bpe.BACKUP_DIR = os.path.join(bpe.BUILD_DIR, "bpe_originals")

            make_sources(root)
            baseline = tree_hashes(root)
            bpe.prepare_backup()
            for rel_path, _ in bpe.BACKUP_FILES:
                with open(os.path.join(root, rel_path), "ab") as output:
                    output.write(b"patched")
            assert bpe.restore_backup()
            assert tree_hashes(root) == baseline
            assert not os.path.exists(bpe.BACKUP_DIR)

            real_build = bpe.build_bpe
            try:
                def fail_after_mutation():
                    with open(os.path.join(root, "src", "spectalk.c"), "ab") as output:
                        output.write(bpe.BPE_POISON_MARKER.encode())
                    raise RuntimeError("injected failure")

                bpe.build_bpe = fail_after_mutation
                try:
                    bpe.main()
                except RuntimeError:
                    pass
                else:
                    raise AssertionError("injected build failure was swallowed")
            finally:
                bpe.build_bpe = real_build
            assert tree_hashes(root) == baseline
            assert not os.path.exists(bpe.BACKUP_DIR)

            bpe.prepare_backup()
            os.remove(os.path.join(bpe.BACKUP_DIR, bpe.BACKUP_FILES[-1][1]))
            live_before = tree_hashes(root)
            try:
                bpe.restore_backup()
            except SystemExit:
                pass
            else:
                raise AssertionError("incomplete backup was accepted")
            assert tree_hashes(root) == live_before
            assert os.path.isdir(bpe.BACKUP_DIR)
    finally:
        bpe.ROOT, bpe.BUILD_DIR, bpe.BPE_STAMP, bpe.BACKUP_DIR = saved

    print("BPE transaction check OK")


if __name__ == "__main__":
    main()
