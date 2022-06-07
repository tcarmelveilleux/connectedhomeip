#pragma once

#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/Span.h>

namespace chip {
namespace Crypto {

class OperationalKeystore
{
public:
    virtual ~OperationalKeystore() {}

    // ==== Commissionable-only API (although can be used by controllers) ====

    /**
     * @brief Returns true if a pending operational key exists from a previous
     *        `NewOpKeypairForFabric` before any `CommitOpKeypairForFabric` or
     *        `RevertOpKeypairForFabric`.
     */
    virtual bool HasPendingOpKeypair() const = 0;

    /**
     * @brief This initializes a new keypair for the given fabric and generates a CSR for it,
     *        so that it can be passed in a CSRResponse.
     *
     * The keypair is temporary and becomes usable for `SignWithOpKeypair` only after either
     * `ActivateOpKeypairForFabric` or `CommitOpKeypairForFabric is called. It is destroyed if
     * `RevertPendingKeypairs` or `Finish` is called before `CommitOpKeypairForFabric`.
     *  If a pending keypair already existed, it is replaced by this call.
     *
     *  Only one pending operational keypair is supported at a time.
     *
     * @param fabricIndex - FabricIndex for which a new keypair must be made available
     * @param certificateSigningRequest - Buffer to contain the CSR. Must be at least `kMAX_CSR_Length` large.
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_BUFFER_TOO_SMALL if `certificateSigningRequest` buffer is too small
     * @retval CHIP_ERROR_INCORRECT_STATE if Init() had not been called or Finish() was already called
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is already a pending keypair for another `fabricIndex` value
     *                                         or if fabricIndex is an invalid value.
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `IngestKeypairForFabric` is supported
     * @retval other CHIP_ERROR value on internal crypto engine errors
     */
    virtual CHIP_ERROR NewOpKeypairForFabric(FabricIndex fabricIndex, MutableByteSpan & outCertificateSigningRequest) = 0;

    /**
     * @brief Temporarily activates the operational keypair last generated with `NewOpKeypairForFabric`, so
     *        that `SignWithOpKeypair` starts using it, but only it matches public key associated
     *        with the last NewOpKeypairForFabric.
     *
     * This is to be used by AddNOC and UpdateNOC so that a prior key generated by NewOpKeypairForFabric
     * can be used for CASE while not committing it yet to permanent storage to remain after fail-safe.
     *
     * @param fabricIndex - FabricIndex for which to activate the keypair, used for security cross-checking
     * @param nocPublicKey - Subject public key associated with an incoming NOC
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if Init() had not been called or Finish() was already called
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is no pending operational keypair for `fabricIndex`
     * @retval CHIP_ERROR_INVALID_PUBLIC_KEY if `nocPublicKey` does not match the public key associated with the
     *                                       public key from last `NewOpKeypairForFabric`.
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `IngestKeypairForFabric` is supported
     * @retval other CHIP_ERROR value on internal crypto engine errors
     */
    virtual CHIP_ERROR ActivateOpKeypairForFabric(FabricIndex fabricIndex, const Crypto::P256PublicKey & nocPublicKey) = 0;

    /**
     * @brief Permanently commit the operational keypair last generated with `NewOpKeypairForFabric`,
     *        replacing a prior one previously committed, if any, so that `SignWithOpKeypair` for the
     *        given `FabricIndex` permanently uses the key that was pending.
     *
     * This is to be used when CommissioningComplete is successfully received
     *
     * @param fabricIndex - FabricIndex for which to commit the keypair, used for security cross-checking
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if Init() had not been called or Finish() was already called,
     *                                    or ActivateOpKeypairForFabric not yet called
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is no pending operational keypair for `fabricIndex`
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `IngestKeypairForFabric` is supported
     * @retval other CHIP_ERROR value on internal crypto engine errors
     */
    virtual CHIP_ERROR CommitOpKeypairForFabric(FabricIndex fabricIndex) = 0;

    /**
     * @brief Permanently release the operational keypair last generated with `NewOpKeypairForFabric`,
     *        such that `SignWithOpKeypair` uses the previously committed key (if one existed).
     *
     * This is to be used when on fail-safe expiry prior to CommissioningComplete.
     *
     * This method cannot error-out and must always succeed, even on a no-op. This should
     * be safe to do given that `CommitOpKeypairForFabric` must succeed to make a new operational
     * keypair usable.
     */
    virtual void RevertPendingKeypairs() = 0;

    // ==== Primary operation required: signature
    /**
     * @brief Sign a message with a fabric's currently-active operational keypair.
     *
     * If a Keypair was successfully made temporarily active with `ActivateOpKeypairForFabric`, then
     * that is the keypair whose private key is used. Otherwise, the last committed private key
     * is used, if one exists
     *
     * @param fabricIndex - FabricIndex whose operational keypair will be used to sign the `message`
     * @param message - Message to sign with the currently active operational keypair
     * @param outSignature - Buffer to contain the signature
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if Init() had not been called or Finish() was already called
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if no active key is found for the given `fabricIndex` or if
     *                                         `fabricIndex` is invalid.
     * @retval other CHIP_ERROR value on internal crypto engine errors
     *
     */
    virtual CHIP_ERROR SignWithOpKeypair(FabricIndex fabricIndex, const ByteSpan & message,
                                         Crypto::P256ECDSASignature & outSignature) const = 0;
};

} // namespace Crypto
} // namespace chip
