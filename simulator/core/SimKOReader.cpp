// KOReaderCredentialStore stub for simulator
#include "KOReaderCredentialStore.h"

KOReaderCredentialStore KOReaderCredentialStore::instance;

bool KOReaderCredentialStore::saveToFile() const { return true; }
bool KOReaderCredentialStore::loadFromFile() { return true; }
bool KOReaderCredentialStore::loadFromBinaryFile() { return false; }
void KOReaderCredentialStore::setCredentials(const std::string&, const std::string&) {}
std::string KOReaderCredentialStore::getMd5Password() const { return ""; }
bool KOReaderCredentialStore::hasCredentials() const { return false; }
void KOReaderCredentialStore::clearCredentials() {}
void KOReaderCredentialStore::setServerUrl(const std::string&) {}
std::string KOReaderCredentialStore::getBaseUrl() const { return ""; }
void KOReaderCredentialStore::setMatchMethod(DocumentMatchMethod) {}
