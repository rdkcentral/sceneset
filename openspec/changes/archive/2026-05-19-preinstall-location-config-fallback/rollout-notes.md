## Startup Log Expectations

- SceneSet logs the selected preinstall directory source at initialization:
  - `/etc/sceneset.conf preinstallLocation`
  - `PreinstallManager plugin config`
  - `compile-time APP_PREINSTALL_DIRECTORY`

## Rollback Approach

- Revert this change set to restore previous behavior:
  - preinstall directory resolution only from dynamic plugin config then compile-time fallback
  - startup force-install decision tied only to FSR marker
- No data migration is required for rollback.
