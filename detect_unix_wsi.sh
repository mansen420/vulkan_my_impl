if [ "$(loginctl show-session $(loginctl user-status $USER | grep -E -m 1 'session-[0-9]+\.scope' | sed -E 's/^.*?session-([0-9]+)\.scope.*$/\1/') -p Type | grep -ic "wayland")" -ge 1 ]; then
    echo "WAYLAND"
else
    echo "X11"
fi