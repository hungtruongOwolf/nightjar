#include "ntfy_sink.h"

#include <curl/curl.h>

#include <cstdio>

#include "nightjar/clip_encoder.h"  // encode_gray_png, stb implementation lives in the core

namespace nightjar {
namespace {

size_t discard_body(char*, size_t size, size_t nmemb, void*) { return size * nmemb; }

const char* subject_word(Subject s) {
    switch (s) {
        case Subject::Person: return "Person";
        case Subject::Vehicle: return "Vehicle";
        case Subject::Animal: return "Animal";
        case Subject::Package: return "Package";
    }
    return "Alert";
}

}  // namespace

NtfyFields ntfy_fields_for(const Alert& alert) {
    NtfyFields f;
    if (alert.degraded) {
        f.title = "Nightjar (motion-only)";
        f.tags = "warning";
        f.priority = "default";
    } else {
        f.title = std::string("Nightjar: ") + subject_word(alert.subject);
        f.tags = "rotating_light";
        f.priority = "high";
    }
    return f;
}

NtfySink::NtfySink(NtfyConfig config) : config_(std::move(config)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void NtfySink::send(const Alert& alert) {
    const std::string url = config_.server + "/" + config_.topic;
    const NtfyFields fields = ntfy_fields_for(alert);
    const std::vector<uint8_t> png =
        alert.image.size > 0 ? encode_gray_png(alert.image.pixels.data(), alert.image.size)
                             : std::vector<uint8_t>{};

    for (int attempt = 0; attempt <= config_.retries; ++attempt) {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        struct curl_slist* headers = nullptr;
        const std::string h_title = "X-Title: " + fields.title;
        const std::string h_msg = "X-Message: " + alert.one_liner;
        const std::string h_tags = "X-Tags: " + fields.tags;
        const std::string h_prio = "X-Priority: " + fields.priority;
        headers = curl_slist_append(headers, h_title.c_str());
        headers = curl_slist_append(headers, h_msg.c_str());
        headers = curl_slist_append(headers, h_tags.c_str());
        headers = curl_slist_append(headers, h_prio.c_str());

        if (!png.empty()) {
            headers = curl_slist_append(headers, "X-Filename: nightjar.png");
            headers = curl_slist_append(headers, "Content-Type: image/png");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, png.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(png.size()));
        } else {
            // No image: the one-liner is the body.
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, alert.one_liner.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config_.timeout_s));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_body);

        const CURLcode rc = curl_easy_perform(curl);
        long status = 0;
        if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        last_status_ = static_cast<int>(status);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (rc == CURLE_OK && status >= 200 && status < 300) return;  // delivered
    }
}

}  // namespace nightjar
