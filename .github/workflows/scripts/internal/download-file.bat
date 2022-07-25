rem download-file(url, file_name, api_key)
rem dg2 machine in RIL has a strange issue:
rem schannel: next InitializeSecurityContext failed: Unknown error (0x80092013)
rem - The revocation function was unable to check revocation because the revocation server was offline.
rem so use --ssl-no-revoke as a temporal workaround.
curl --ssl-no-revoke --connect-timeout 5 --max-time 3600 --retry 5 --retry-delay 0 --retry-max-time 40 --fail -H "X-JFrog-Art-Api:%3" "%1/%2" --output %2
exit /b %errorlevel%
