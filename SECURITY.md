# Security

## What the checks do

- They detect a damaged or changed zip before anything is written.
- They stop a wrong phone model.
- They verify the written recovery by reading it back.

## What the checks do not do

The SHA-256 values are built into the installer. They are not signed.
Someone who edits the installer binary can bypass them.
They guard against accidents and careless repacking. They are not a security boundary.

## Permissive SELinux

This recovery sets SELinux to permissive. This is common for custom recovery.
It is only active inside recovery.

## Safe use

- Download only from this repo's Releases page.
- Compare the SHA-256 value before you flash.
- Do not flash files from unknown sources.

## Report a problem

Send a message on Telegram: https://t.me/ibralyam
