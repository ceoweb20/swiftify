//! Read-only discovery through the existing playback session. Spotify's public
//! playlist API does not expose the personalized home / Made For You shelves.
//! Query hashes are discovered from Spotify's own public JS (never executed).
//! No cookie, external hash service, or account token is sent to a third party.

use std::{
    collections::HashSet,
    io::Read,
    time::{Duration, Instant},
};

use librespot::{core::Session, protocol::playlist4_external::SelectedListContent};
use protobuf::Message;
use regex::Regex;
use reqwest::{Client, Url};
use serde_json::{Value, json};
use tokio::sync::Mutex;

const ASSETS: &str = "https://open.spotifycdn.com/cdn/build/web-player/";
const QUERY: &str = "https://api-partner.spotify.com/pathfinder/v2/query";
const MADE_FOR_YOU: &str = "spotify:page:0JQ5DAt0tbjZptfcdMSKl3";
const MAX_BYTES: usize = 8 * 1024 * 1024;
const USER_AGENT: &str = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

#[derive(Clone)]
struct Hashes {
    home: String,
    browse: String,
    fetched: Instant,
}

static HASHES: Mutex<Option<Hashes>> = Mutex::const_new(None);

fn client() -> Result<Client, String> {
    Client::builder()
        .user_agent(USER_AGENT)
        .timeout(Duration::from_secs(20))
        .redirect(reqwest::redirect::Policy::none())
        .build()
        .map_err(|_| "Could not initialize Spotify discovery networking.".to_owned())
}

async fn body(mut response: reqwest::Response) -> Result<Vec<u8>, String> {
    if !response.status().is_success() {
        return Err(format!(
            "Spotify discovery request failed (HTTP {}).",
            response.status().as_u16()
        ));
    }
    let mut bytes = Vec::new();
    while let Some(chunk) = response
        .chunk()
        .await
        .map_err(|_| "Could not read Spotify discovery response.")?
    {
        if bytes.len() + chunk.len() > MAX_BYTES {
            return Err("Spotify discovery response is too large.".to_owned());
        }
        bytes.extend_from_slice(&chunk);
    }
    if bytes.starts_with(&[0x1f, 0x8b]) {
        let mut decoded = Vec::new();
        flate2::read::GzDecoder::new(bytes.as_slice())
            .take(MAX_BYTES as u64 + 1)
            .read_to_end(&mut decoded)
            .map_err(|_| "Could not decode Spotify discovery response.")?;
        if decoded.len() > MAX_BYTES {
            return Err("Spotify discovery response is too large.".to_owned());
        }
        bytes = decoded;
    }
    Ok(bytes)
}

async fn public_text(client: &Client, url: &str) -> Result<String, String> {
    let url = Url::parse(url).map_err(|_| "Invalid Spotify asset URL.")?;
    if url.scheme() != "https"
        || !matches!(
            url.host_str(),
            Some("open.spotify.com" | "open.spotifycdn.com")
        )
    {
        return Err("Untrusted Spotify asset URL.".to_owned());
    }
    let response = client
        .get(url)
        .header("accept-encoding", "identity")
        .send()
        .await
        .map_err(|_| "Could not fetch Spotify discovery definitions.")?;
    String::from_utf8(body(response).await?)
        .map_err(|_| "Invalid Spotify discovery definitions.".to_owned())
}

fn operation_hash(script: &str, operation: &str) -> Option<String> {
    let pattern = format!(
        r#""{}","query","([a-fA-F0-9]{{64}})""#,
        regex::escape(operation)
    );
    Regex::new(&pattern)
        .ok()?
        .captures(script)?
        .get(1)
        .map(|value| value.as_str().to_owned())
}

fn main_bundle(html: &str) -> Option<String> {
    Regex::new(r"https://open\.spotifycdn\.com/cdn/build/web-player/web-player\.[A-Za-z0-9_-]+\.js")
        .ok()?
        .find(html)
        .map(|value| value.as_str().to_owned())
}

fn browse_bundle(script: &str) -> Option<String> {
    let id = Regex::new(r#"\b([0-9]+):"browse-v2""#)
        .ok()?
        .captures(script)?
        .get(1)?
        .as_str();
    let pattern = format!(r#"\b{id}:"([a-fA-F0-9]{{8,64}})""#);
    let hash = Regex::new(&pattern)
        .ok()?
        .captures(script)?
        .get(1)?
        .as_str()
        .to_owned();
    Some(format!("{ASSETS}browse-v2.{hash}.js"))
}

async fn hashes(client: &Client) -> Result<Hashes, String> {
    let mut cache = HASHES.lock().await;
    if let Some(value) = cache
        .as_ref()
        .filter(|value| value.fetched.elapsed() < Duration::from_secs(86400))
    {
        return Ok(value.clone());
    }
    let found = async {
        let html = public_text(client, "https://open.spotify.com").await?;
        let main_url = main_bundle(&html).ok_or("Spotify's player bundle could not be found.")?;
        let script = public_text(client, &main_url).await?;
        let home =
            operation_hash(&script, "home").ok_or("Spotify's home query could not be found.")?;
        let browse = match operation_hash(&script, "browsePage") {
            Some(hash) => hash,
            None => {
                let url =
                    browse_bundle(&script).ok_or("Spotify's browse bundle could not be found.")?;
                operation_hash(&public_text(client, &url).await?, "browsePage")
                    .ok_or("Spotify's browse query could not be found.")?
            }
        };
        Ok::<_, String>(Hashes {
            home,
            browse,
            fetched: Instant::now(),
        })
    }
    .await;
    match found {
        Ok(value) => {
            *cache = Some(value.clone());
            Ok(value)
        }
        Err(error) => cache.clone().ok_or(error),
    }
}

fn query_data(bytes: &[u8]) -> Result<Value, String> {
    let value: Value =
        serde_json::from_slice(bytes).map_err(|_| "Invalid Spotify discovery response.")?;
    if value
        .get("errors")
        .and_then(Value::as_array)
        .is_some_and(|errors| !errors.is_empty())
    {
        return Err(
            "Spotify could not load personalized collections. Its internal API may have changed."
                .to_owned(),
        );
    }
    value
        .get("data")
        .filter(|data| data.is_object())
        .cloned()
        .ok_or_else(|| "Spotify returned no personalized collections.".to_owned())
}

async fn query(
    session: &Session,
    client: &Client,
    name: &str,
    hash: &str,
    variables: Value,
    locale: &str,
) -> Result<Value, String> {
    let token = session
        .login5()
        .auth_token()
        .await
        .map_err(|_| "Could not authorize Spotify discovery.")?;
    let client_token = session
        .spclient()
        .client_token()
        .await
        .map_err(|_| "Could not authorize Spotify discovery client.")?;
    let request = json!({"operationName":name, "variables":variables,
        "extensions":{"persistedQuery":{"version":1,"sha256Hash":hash}}});
    let response = client
        .post(QUERY)
        .header(
            "authorization",
            format!("{} {}", token.token_type, token.access_token),
        )
        .header("client-token", client_token)
        .header("app-platform", "WebPlayer")
        .header("spotify-app-version", "896000000")
        .header("accept-language", locale)
        .header("content-type", "application/json")
        .body(request.to_string())
        .send()
        .await
        .map_err(|_| "Could not load Spotify personalized collections.")?;
    let bytes = body(response).await?;
    let parsed: Value = serde_json::from_slice(&bytes).unwrap_or_default();
    if parsed
        .get("errors")
        .and_then(Value::as_array)
        .is_some_and(|errors| {
            errors.iter().any(|error| {
                error
                    .get("message")
                    .and_then(Value::as_str)
                    .is_some_and(|message| message.to_ascii_lowercase().contains("persistedquery"))
            })
        })
    {
        // The next explicit refresh rediscovers definitions from Spotify.
        *HASHES.lock().await = None;
    }
    query_data(&bytes)
}

fn playlist_id(uri: &str) -> Option<&str> {
    let id = uri.strip_prefix("spotify:playlist:")?;
    (id.len() == 22 && id.bytes().all(|byte| byte.is_ascii_alphanumeric())).then_some(id)
}

fn image(sources: &Value) -> Option<String> {
    sources
        .as_array()?
        .iter()
        .filter_map(|source| source.get("url")?.as_str())
        .find(|url| url.starts_with("https://"))
        .map(str::to_owned)
}

fn playlist_card(item: &Value) -> Option<Value> {
    let data = item.pointer("/content/data")?;
    if data.get("__typename")?.as_str()? != "Playlist" {
        return None;
    }
    let uri = item
        .get("uri")
        .and_then(Value::as_str)
        .or_else(|| data.get("uri")?.as_str())?;
    let id = playlist_id(uri)?;
    let name = data.get("name").and_then(Value::as_str).unwrap_or_default();
    let cover = data
        .pointer("/images/items")
        .and_then(Value::as_array)
        .and_then(|items| items.iter().find_map(|art| image(&art["sources"])));
    Some(json!({"id":id,"uri":uri,"name":name,
        "description":data.get("description").and_then(Value::as_str).unwrap_or_default(),
        "owner":{"id":"","display_name":data.pointer("/ownerV2/data/name").and_then(Value::as_str).unwrap_or("Spotify")},
        "images":cover.map(|url|vec![json!({"url":url})]).unwrap_or_default(),
        "items":{"total":0},"public":false,"collaborative":false}))
}

fn sections(container: &Value) -> Vec<Value> {
    let Some(items) = container
        .pointer("/sections/items")
        .and_then(Value::as_array)
    else {
        return Vec::new();
    };
    items
        .iter()
        .filter_map(|section| {
            let title = section
                .pointer("/data/title/transformedLabel")
                .and_then(Value::as_str)
                .unwrap_or_default();
            let mut seen = HashSet::new();
            let playlists: Vec<Value> = section
                .pointer("/sectionItems/items")
                .and_then(Value::as_array)?
                .iter()
                .filter_map(playlist_card)
                .filter(|playlist| {
                    seen.insert(playlist["id"].as_str().unwrap_or_default().to_owned())
                })
                .collect();
            (!playlists.is_empty()).then(|| json!({"title":title,"items":playlists}))
        })
        .collect()
}

async fn personalized(session: &Session, locale: &str) -> Result<(Vec<Value>, bool), String> {
    let client = client()?;
    let hashes = hashes(&client).await?;
    let time_zone = iana_time_zone::get_timezone().unwrap_or_else(|_| "UTC".to_owned());
    let (home, made) = tokio::join!(
        query(
            session,
            &client,
            "home",
            &hashes.home,
            json!({
            "homeEndUserIntegration":"INTEGRATION_WEB_PLAYER", "timeZone":time_zone, "sp_t":"", "facet":"",
            "sectionItemsLimit":50, "includeEpisodeContentRatingsV2":false}),
            locale
        ),
        query(
            session,
            &client,
            "browsePage",
            &hashes.browse,
            json!({
            "uri":MADE_FOR_YOU, "pagePagination":{"offset":0,"limit":50},
            "sectionPagination":{"offset":0,"limit":50}, "browseEndUserIntegration":"INTEGRATION_WEB_PLAYER",
            "includeEpisodeContentRatingsV2":false}),
            locale
        )
    );
    let mut shelves = Vec::new();
    let mut available = false;
    if let Ok(data) = &made {
        if let Some(container) = data.get("browse").filter(|value| value.is_object()) {
            shelves.extend(sections(container));
            available = true;
        }
    }
    if let Ok(data) = &home {
        if let Some(container) = data
            .pointer("/home/sectionContainer")
            .filter(|value| value.is_object())
        {
            shelves.extend(sections(container));
            available = true;
        }
    }
    if !available {
        return Err("Spotify personalized collections are temporarily unavailable.".to_owned());
    }
    // Deduplicate by URI, not by title: two different Daily Mixes may share a name.
    let mut seen = HashSet::new();
    for shelf in &mut shelves {
        if let Some(items) = shelf["items"].as_array_mut() {
            items.retain(|item| seen.insert(item["id"].as_str().unwrap_or_default().to_owned()));
        }
    }
    shelves.retain(|shelf| {
        shelf["items"]
            .as_array()
            .is_some_and(|items| !items.is_empty())
    });
    // Some personalized cards intentionally omit their name and cover. Resolve
    // only those headers through the session, four at a time, never whole tracks.
    let blanks: Vec<String> = shelves
        .iter()
        .flat_map(|shelf| shelf["items"].as_array().into_iter().flatten())
        .filter(|item| item["name"].as_str().unwrap_or_default().trim().is_empty())
        .filter_map(|item| item["id"].as_str().map(str::to_owned))
        .collect();
    let mut named = std::collections::HashMap::new();
    for batch in blanks.chunks(4) {
        let mut pending = tokio::task::JoinSet::new();
        for id in batch {
            let session = session.clone();
            let id = id.clone();
            pending.spawn(async move {
                let endpoint = format!("/playlist/v2/playlist/{id}?from=0&length=0");
                let bytes = session
                    .spclient()
                    .request(&http::Method::GET, &endpoint, None, None)
                    .await
                    .ok()?;
                let list = SelectedListContent::parse_from_bytes(&bytes).ok()?;
                let attributes = list.attributes.get_or_default();
                if attributes.name().trim().is_empty() {
                    return None;
                }
                let cover = attributes
                    .picture_size
                    .iter()
                    .map(|image| image.url())
                    .find(|url| url.starts_with("https://"))
                    .map(str::to_owned)
                    .or_else(|| {
                        (!attributes.picture().is_empty()).then(|| {
                            format!(
                                "https://i.scdn.co/image/{}",
                                attributes
                                    .picture()
                                    .iter()
                                    .map(|byte| format!("{byte:02x}"))
                                    .collect::<String>()
                            )
                        })
                    });
                Some((id, attributes.name().to_owned(), cover))
            });
        }
        while let Some(result) = pending.join_next().await {
            if let Ok(Some((id, name, cover))) = result {
                named.insert(id, (name, cover));
            }
        }
    }
    for shelf in &mut shelves {
        if let Some(items) = shelf["items"].as_array_mut() {
            for item in items.iter_mut() {
                if let Some((name, cover)) = item["id"].as_str().and_then(|id| named.get(id)) {
                    item["name"] = json!(name);
                    if let Some(url) = cover {
                        item["images"] = json!([{"url":url}]);
                    }
                }
            }
            items.retain(|item| !item["name"].as_str().unwrap_or_default().trim().is_empty());
        }
    }
    shelves.retain(|shelf| {
        shelf["items"]
            .as_array()
            .is_some_and(|items| !items.is_empty())
    });
    Ok((shelves, home.is_err() || made.is_err()))
}

fn path_segment(value: &str) -> String {
    value
        .bytes()
        .map(|byte| {
            if byte.is_ascii_alphanumeric() || b"-_.~".contains(&byte) {
                (byte as char).to_string()
            } else {
                format!("%{byte:02X}")
            }
        })
        .collect()
}

fn root_page(list: &SelectedListContent) -> Vec<Value> {
    let contents = list.contents.get_or_default();
    contents.items.iter().enumerate().filter_map(|(index, item)| {
        let id = playlist_id(item.uri())?;
        let meta = contents.meta_items.get(index)?;
        let attributes = meta.attributes.get_or_default();
        if attributes.name().trim().is_empty() { return None; }
        let cover = attributes.picture_size.iter().map(|image| image.url())
            .find(|url| url.starts_with("https://")).map(str::to_owned).or_else(|| {
                (!attributes.picture().is_empty()).then(|| format!("https://i.scdn.co/image/{}",
                    attributes.picture().iter().map(|byte| format!("{byte:02x}")).collect::<String>()))
            });
        Some(json!({"id":id,"uri":item.uri(),"name":attributes.name(),"description":attributes.description(),
            "owner":{"id":meta.owner_username(),"display_name":meta.owner_username()},
            "images":cover.map(|url|vec![json!({"url":url})]).unwrap_or_default(),
            "items":{"total":meta.length().max(0)},"public":item.attributes.public(),
            "collaborative":attributes.collaborative()}))
    }).collect()
}

async fn rootlist(session: &Session) -> Result<Vec<Value>, String> {
    let mut offset = 0usize;
    let mut result = Vec::new();
    let mut seen = HashSet::new();
    loop {
        let endpoint = format!(
            "/playlist/v2/user/{}/rootlist?decorate=revision,attributes,length,owner,capabilities,status_code&from={offset}&length=300",
            path_segment(&session.username())
        );
        let bytes = session
            .spclient()
            .request(&http::Method::GET, &endpoint, None, None)
            .await
            .map_err(|_| "Could not load Spotify's saved playlists.")?;
        let list = SelectedListContent::parse_from_bytes(&bytes)
            .map_err(|_| "Invalid Spotify rootlist response.")?;
        let content = list.contents.get_or_default();
        if content.pos() < 0 || content.pos() as usize != offset {
            return Err("Spotify returned an unexpected rootlist page.".to_owned());
        }
        result.extend(root_page(&list).into_iter().filter(|playlist| {
            seen.insert(playlist["id"].as_str().unwrap_or_default().to_owned())
        }));
        let count = content.items.len();
        offset += count;
        if count == 0 || !content.truncated() {
            return Ok(result);
        }
        if offset > 100000 {
            return Err("Spotify's rootlist exceeds the supported size.".to_owned());
        }
    }
}

pub async fn load(session: &Session, locale: &str) -> Result<String, String> {
    let language = if locale.starts_with("ru") { "ru" } else { "en" };
    let (personalized, library) = tokio::join!(personalized(session, language), rootlist(session));
    if personalized.is_err() && library.is_err() {
        return Err("Spotify collections could not be loaded. Try again later.".to_owned());
    }
    let personalized_available = personalized.is_ok();
    let rootlist_available = library.is_ok();
    let (shelves, partial) = personalized.unwrap_or_default();
    Ok(json!({"sections":shelves,"library":library.unwrap_or_default(),"userId":session.username(),
        "personalizedAvailable":personalized_available,"libraryAvailable":rootlist_available,
        "partial":partial || !personalized_available || !rootlist_available}).to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_only_spotify_public_assets_and_valid_hashes() {
        assert!(main_bundle(r#"<script src="https://evil.example/web-player.a.js">"#).is_none());
        assert_eq!(
            main_bundle(
                r#"<script src="https://open.spotifycdn.com/cdn/build/web-player/web-player.a.js">"#
            )
            .as_deref(),
            Some("https://open.spotifycdn.com/cdn/build/web-player/web-player.a.js")
        );
        let script = format!(
            r#"Query("home","query","{}",null);3719:"browse-v2",3719:"df02045f""#,
            "a".repeat(64)
        );
        assert_eq!(operation_hash(&script, "home"), Some("a".repeat(64)));
        assert!(operation_hash(&script, "browsePage").is_none());
        assert_eq!(
            browse_bundle(&script).as_deref(),
            Some("https://open.spotifycdn.com/cdn/build/web-player/browse-v2.df02045f.js")
        );
        assert!(operation_hash(r#"Query("home","query","bad",null)"#, "home").is_none());
    }

    #[test]
    fn rejects_graphql_errors_even_with_http_success() {
        assert!(query_data(br#"{"data":null,"errors":[{"message":"error"}]}"#).is_err());
        assert!(query_data(br#"{"data":{"home":{}},"errors":[{"message":"partial"}]}"#).is_err());
        assert!(query_data(br#"{"data":{"home":{}}}"#).is_ok());
    }

    #[test]
    fn playlist_cards_keep_unicode_and_skip_nonplaylists() {
        let card = json!({"uri":"spotify:playlist:37i9dQZF1DXcBWIGoYBM5M", "content":{"data":{
            "__typename":"Playlist","name":"Синтез 🎧", "images":{"items":[{"sources":[{"url":"https://i.scdn.co/image/cover"}]}]},
            "ownerV2":{"data":{"name":"Spotify"}}}}});
        let found = playlist_card(&card).unwrap();
        assert_eq!(found["name"], "Синтез 🎧");
        assert_eq!(found["uri"], card["uri"]);
        assert_eq!(found["images"][0]["url"], "https://i.scdn.co/image/cover");
        assert!(
            playlist_card(
                &json!({"uri":"spotify:app:dj","content":{"data":{"__typename":"Unknown"}}})
            )
            .is_none()
        );
        assert!(playlist_id("spotify:playlist:../../secret").is_none());
        assert_eq!(path_segment("имя user"), "%D0%B8%D0%BC%D1%8F%20user");
    }

    #[test]
    fn shelves_skip_unavailable_entities_and_duplicate_uris() {
        let card = json!({"uri":"spotify:playlist:37i9dQZF1DXcBWIGoYBM5M", "content":{"data":{
            "__typename":"Playlist","name":"Daily Mix"}}});
        let container = json!({"sections":{"items":[{"data":{"title":{"transformedLabel":"Made for you"}},
            "sectionItems":{"items":[card.clone(),card,{"uri":"spotify:album:x","content":{"data":{"__typename":"Album"}}}]}}]}});
        let shelves = sections(&container);
        assert_eq!(shelves.len(), 1);
        assert_eq!(shelves[0]["items"].as_array().unwrap().len(), 1);
        assert_eq!(shelves[0]["title"], "Made for you");
    }

    #[tokio::test]
    #[ignore = "public Spotify web assets; no login or account data"]
    async fn live_query_definitions() {
        let found = hashes(&client().unwrap()).await.unwrap();
        assert_eq!(found.home.len(), 64);
        assert_eq!(found.browse.len(), 64);
    }

    #[test]
    fn rootlist_reads_playlists_inside_folders_without_turning_folders_into_playlists() {
        let mut list = SelectedListContent::new();
        let contents = list.contents.mut_or_insert_default();
        for uri in [
            "spotify:start-group:folder",
            "spotify:playlist:37i9dQZF1DXcBWIGoYBM5M",
            "spotify:end-group:folder",
        ] {
            contents.items.push(Default::default());
            contents.items.last_mut().unwrap().set_uri(uri.to_owned());
            contents.meta_items.push(Default::default());
        }
        let metadata = &mut contents.meta_items[1];
        metadata.set_owner_username("user".to_owned());
        metadata.set_length(42);
        metadata
            .attributes
            .mut_or_insert_default()
            .set_name("Синтез".to_owned());
        let found = root_page(&list);
        assert_eq!(found.len(), 1);
        assert_eq!(found[0]["name"], "Синтез");
        assert_eq!(found[0]["items"]["total"], 42);
        assert_eq!(found[0]["owner"]["id"], "user");
    }
}
