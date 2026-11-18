#pragma once

#include <winerror.h>

namespace gazer {

// WinVerifyTrust codes where the embedded signature still matches the file.
// A self-signed certificate is accepted. Callers also match the signer
// thumbprint to the installed binary. A missing signature or a hash mismatch
// stays rejected.
inline bool signatureIntact(long status)
{
    return status == ERROR_SUCCESS || status == TRUST_E_SUBJECT_NOT_TRUSTED
           || status == CERT_E_UNTRUSTEDROOT || status == CERT_E_UNTRUSTEDTESTROOT;
}

} // namespace gazer
