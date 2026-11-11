## Rollout Notes

### EntOS Provisioning Expectation
- EntOS images should provision /etc/sceneset.conf with defaultHomeApp set to the EPG appId.
- Example:
  - defaultHomeApp=org.rdk.EPG

### Runtime Precedence
1. Base config: /etc/sceneset.conf
2. Optional override: /opt/sceneset.conf (applied only when ENABLE_CONFIG_OVERRIDE is enabled)
3. Legacy fallback for home app selection when defaultHomeApp is unavailable:
  - /opt/sceneset_app.conf first line
  - compile-time SCENESET_DEFAULT_APPNAME

### Merge Semantics
- Overlay is key-based: keys in /opt/sceneset.conf override matching keys from /etc/sceneset.conf.
- Keys not present in /opt/sceneset.conf retain their /etc/sceneset.conf values.

### Compatibility Behavior
- Missing or unreadable /etc/sceneset.conf is non-fatal.
- Missing defaultHomeApp preserves existing startup behavior.
- Non-debug builds ignore /opt/sceneset.conf even if the file is present.

### Verification Summary
- Unit tests added/updated for:
  - defaultHomeApp resolution from /etc/sceneset.conf
  - key-overlay behavior for base and override files
  - non-debug ignore behavior for /opt override
  - missing /etc fallback behavior
  - debug-build defaultHomeApp override behavior (Debug config)
