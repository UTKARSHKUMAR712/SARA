(async function () {

    while (
        !Spicetify?.Platform ||
        !Spicetify?.Player ||
        !Spicetify?.showNotification
    ) {
        await new Promise(r => setTimeout(r, 300));
    }

    Spicetify.showNotification(
        "SARA Ultimate Runtime 🚀"
    );

    let ws = null;

    // =====================================
    // SEARCH HELPER
    // =====================================

    async function searchAndExtract(query, type = "track") {

        try {

            await Spicetify.Platform.History.push(
                `/search/${encodeURIComponent(query)}`
            );

            await new Promise(r => setTimeout(r, 2500));

            const links =
                document.querySelectorAll("a");

            for (const link of links) {

                const href =
                    link.getAttribute("href");

                if (!href)
                    continue;

                // TRACK
                if (
                    type === "track" &&
                    href.includes("/track/")
                ) {

                    const id =
                        href.split("/track/")[1]
                            .split("?")[0]
                            .split("/")[0];

                    return `spotify:track:${id}`;
                }

                // PLAYLIST
                if (
                    type === "playlist" &&
                    href.includes("/playlist/")
                ) {

                    const id =
                        href.split("/playlist/")[1]
                            .split("?")[0]
                            .split("/")[0];

                    return `spotify:playlist:${id}`;
                }

                // ARTIST
                if (
                    type === "artist" &&
                    href.includes("/artist/")
                ) {

                    const id =
                        href.split("/artist/")[1]
                            .split("?")[0]
                            .split("/")[0];

                    return `spotify:artist:${id}`;
                }
            }

            return null;

        }
        catch(err) {

            console.error(err);
            return null;
        }
    }

    // =====================================
    // PLAY SONG
    // =====================================

    async function playSong(query) {

        const uri =
            await searchAndExtract(query);

        if (!uri) {

            Spicetify.showNotification(
                "Track not found ❌"
            );

            return;
        }

        await Spicetify.Player.playUri(uri);

        Spicetify.showNotification(
            `Playing ${query} 🎵`
        );
    }

    // =====================================
    // PLAYLIST
    // =====================================

    async function playPlaylist(query) {

        const uri =
            await searchAndExtract(
                query,
                "playlist"
            );

        if (!uri) {

            Spicetify.showNotification(
                "Playlist not found ❌"
            );

            return;
        }

        await Spicetify.Player.playUri(uri);

        Spicetify.showNotification(
            `Playlist: ${query} 🎶`
        );
    }

    // =====================================
    // ARTIST RADIO
    // =====================================

    async function playRadio(query) {

        const artistUri =
            await searchAndExtract(
                query,
                "artist"
            );

        if (!artistUri) {

            Spicetify.showNotification(
                "Artist not found ❌"
            );

            return;
        }

        // Radio URI
        const radioUri =
            artistUri.replace(
                "artist",
                "station:artist"
            );

        await Spicetify.Player.playUri(
            radioUri
        );

        Spicetify.showNotification(
            `Radio: ${query} 📻`
        );
    }

    // =====================================
    // QUEUE
    // =====================================

    async function queueSong(query) {

        try {

            const uri =
                await searchAndExtract(query);

            if (!uri) {

                Spicetify.showNotification(
                    "Queue failed ❌"
                );

                return;
            }

            // Queue API
            if (
                Spicetify.Platform
                    ?.PlayerAPI
                    ?.queue
            ) {

                await Spicetify.Platform
                    .PlayerAPI
                    .queue(uri);

                Spicetify.showNotification(
                    `Queued: ${query} ➕`
                );

                return;
            }

            // Fallback
            await Spicetify.Player.playUri(uri);

            Spicetify.showNotification(
                "Queue fallback used ⚠"
            );

        }
        catch(err) {

            console.error(err);

            Spicetify.showNotification(
                "Queue error ❌"
            );
        }
    }

    // =====================================
    // METADATA
    // =====================================

    function sendMetadata() {

        try {

            const data =
                Spicetify.Player.data;

            if (!data?.item)
                return;

            const meta = {

                title:
                    data.item.name,

                artist:
                    data.item.artists
                        ?.map(a => a.name)
                        .join(", "),

                album:
                    data.item.album?.name,

                duration:
                    Spicetify.Player.getDuration(),

                progress:
                    Spicetify.Player.getProgress(),

                volume:
                    Math.floor(
                        Spicetify.Player.getVolume() * 100
                    ),

                playing:
                    Spicetify.Player.isPlaying(),

                shuffle:
                    Spicetify.Player.getShuffle(),

                repeat:
                    Spicetify.Player.getRepeat(),

                heart:
                    Spicetify.Player.getHeart()
            };

            if (
                ws &&
                ws.readyState === 1
            ) {

                ws.send(JSON.stringify({
                    type: "metadata",
                    data: meta
                }));
            }

        }
        catch(err) {

            console.error(err);
        }
    }

    // =====================================
    // LIVE EVENTS
    // =====================================

    Spicetify.Player.addEventListener(
        "songchange",
        sendMetadata
    );

    Spicetify.Player.addEventListener(
        "onplaypause",
        sendMetadata
    );

    setInterval(sendMetadata, 1000);

    // =====================================
    // WEBSOCKET
    // =====================================

    function connectWebSocket() {

        ws =
            new WebSocket(
                "ws://127.0.0.1:8765"
            );

        ws.onopen = () => {

            Spicetify.showNotification(
                "WS Connected ✅"
            );

            sendMetadata();
        };

        ws.onerror = () => {

            Spicetify.showNotification(
                "WS Error ❌"
            );
        };

        ws.onclose = () => {

            setTimeout(
                connectWebSocket,
                3000
            );
        };

        ws.onmessage = async (event) => {

            try {

                const cmd =
                    JSON.parse(event.data);

                Spicetify.showNotification(
                    `CMD: ${cmd.action}`
                );

                switch(cmd.action) {

                    // PLAYBACK

                    case "play":
                        await playSong(cmd.query);
                        break;

                    case "pause":
                        Spicetify.Player.pause();
                        break;

                    case "resume":
                        Spicetify.Player.play();
                        break;

                    case "next":
                        Spicetify.Player.next();
                        break;

                    case "prev":
                        Spicetify.Player.back();
                        break;

                    case "toggleplay":
                        Spicetify.Player.togglePlay();
                        break;

                    // SEEK

                    case "seek":
                        Spicetify.Player.seek(cmd.ms);
                        break;

                    case "forward":
                        Spicetify.Player.seek(
                            Spicetify.Player.getProgress()
                            + cmd.ms
                        );
                        break;

                    case "backward":
                        Spicetify.Player.seek(
                            Spicetify.Player.getProgress()
                            - cmd.ms
                        );
                        break;

                    // VOLUME

                    case "volume":
                        Spicetify.Player.setVolume(
                            cmd.value / 100
                        );
                        break;

                    case "mute":
                        Spicetify.Player.setMute(true);
                        break;

                    case "unmute":
                        Spicetify.Player.setMute(false);
                        break;

                    case "togglemute":
                        Spicetify.Player.toggleMute();
                        break;

                    // HEART

                    case "heart":
                        Spicetify.Player.setHeart(true);
                        break;

                    case "unheart":
                        Spicetify.Player.setHeart(false);
                        break;

                    case "toggleheart":
                        Spicetify.Player.toggleHeart();
                        break;

                    // SHUFFLE

                    case "shuffle_on":
                        Spicetify.Player.setShuffle(true);
                        break;

                    case "shuffle_off":
                        Spicetify.Player.setShuffle(false);
                        break;

                    // REPEAT

                    case "repeat_off":
                        Spicetify.Player.setRepeat(0);
                        break;

                    case "repeat_all":
                        Spicetify.Player.setRepeat(1);
                        break;

                    case "repeat_one":
                        Spicetify.Player.setRepeat(2);
                        break;

                    // ADVANCED

                    case "queue":
                        await queueSong(cmd.query);
                        break;

                    case "playlist":
                        await playPlaylist(cmd.query);
                        break;

                    case "radio":
                        await playRadio(cmd.query);
                        break;

                    case "metadata":
                        sendMetadata();
                        break;
                }

            }
            catch(err) {

                console.error(err);

                Spicetify.showNotification(
                    "CMD ERROR ❌"
                );
            }
        };
    }

    connectWebSocket();

})();