@ECHO OFF

echo This script is only for developers who are publishing new builds on github.  Most users don't need it

for /f %%i in ('python bin\buildinfo.py long') do set VERSION=%%i
echo Version is %VERSION%

REM Must have a V prefix to trigger github
git tag "v%VERSION%"
git push origin "v%VERSION%"

echo Tag %VERSION% pushed to github, github actions should now be building the draft release.  If it seems good, click to publish it
