/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_APP_WEBRTC_WEBRTCSESSION_H_
#define TALK_APP_WEBRTC_WEBRTCSESSION_H_

#include <string>

#include "talk/app/webrtc/jsep.h"
#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/transportdescriptionfactory.h"
#include "talk/session/media/mediasession.h"

namespace cricket {

class ChannelManager;
class DataChannel;
class StatsReport;
class Transport;
class VideoCapturer;
class BaseChannel;
class VideoChannel;
class VoiceChannel;

}  // namespace cricket

namespace webrtc {

class MediaStreamSignaling;

class WebRtcSession : public cricket::BaseSession,
                      public AudioProviderInterface,
                      public DataChannelFactory,
                      public VideoProviderInterface {
 public:
  WebRtcSession(cricket::ChannelManager* channel_manager,
                talk_base::Thread* signaling_thread,
                talk_base::Thread* worker_thread,
                cricket::PortAllocator* port_allocator,
                MediaStreamSignaling* mediastream_signaling);
  virtual ~WebRtcSession();

  bool Initialize(const MediaConstraintsInterface* constraints);

  void RegisterObserver(IceCandidateObserver* observer) {
    ice_observer_ = observer;
  }

  cricket::VoiceChannel* voice_channel() {
    return voice_channel_.get();
  }
  cricket::VideoChannel* video_channel() {
    return video_channel_.get();
  }
  cricket::DataChannel* data_channel() {
    return data_channel_.get();
  }

  void set_secure_policy(cricket::SecureMediaPolicy secure_policy);
  cricket::SecureMediaPolicy secure_policy() const {
    return session_desc_factory_.secure();
  }

  // Generic error message callback from WebRtcSession.
  // TODO - It may be necessary to supply error code as well.
  sigslot::signal0<> SignalError;

  SessionDescriptionInterface* CreateOffer(const MediaHints& hints);
  SessionDescriptionInterface* CreateOffer(
      const MediaConstraintsInterface* constraints);
  SessionDescriptionInterface* CreateAnswer(
      const MediaHints& hints,
      const SessionDescriptionInterface* offer);
  SessionDescriptionInterface* CreateAnswer(
      const MediaConstraintsInterface* constraints,
      const SessionDescriptionInterface* offer);

  bool SetLocalDescription(SessionDescriptionInterface* desc);
  bool SetRemoteDescription(SessionDescriptionInterface* desc);
  bool ProcessIceMessage(const IceCandidateInterface* ice_candidate);
  const SessionDescriptionInterface* local_description() const {
    return local_desc_.get();
  }
  const SessionDescriptionInterface* remote_description() const {
    return remote_desc_.get();
  }

  bool GetLocalTrackName(uint32 ssrc, std::string* name);
  bool GetRemoteTrackName(uint32 ssrc, std::string* name);

  // AudioMediaProviderInterface implementation.
  virtual void SetAudioPlayout(const std::string& name, bool enable);
  virtual void SetAudioSend(const std::string& name, bool enable);

  // Implements VideoMediaProviderInterface.
  virtual bool SetCaptureDevice(const std::string& name,
                                cricket::VideoCapturer* camera);
  virtual void SetVideoPlayout(const std::string& name,
                               bool enable,
                               cricket::VideoRenderer* renderer);
  virtual void SetVideoSend(const std::string& name, bool enable);

  virtual bool CanSendDtmf(const std::string& name);
  virtual bool SendDtmf(const std::string& send_name,
                        const std::string& tones, int duration,
                        const std::string& play_name);

  talk_base::scoped_refptr<DataChannel> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config);

 private:
  // Indicates the type of SessionDescription in a call to SetLocalDescription
  // and SetRemoteDescription.
  enum Action {
    kOffer,
    kPrAnswer,
    kAnswer,
  };
  // Invokes ConnectChannels() on transport proxies, which initiates ice
  // candidates allocation.
  bool StartCandidatesAllocation();
  bool UpdateSessionState(Action action, cricket::ContentSource source,
                          const cricket::SessionDescription* desc);
  static Action GetAction(const std::string& type);

  virtual void OnMessage(talk_base::Message* msg);

  // Transport related callbacks, override from cricket::BaseSession.
  virtual void OnTransportRequestSignaling(cricket::Transport* transport);
  virtual void OnTransportConnecting(cricket::Transport* transport);
  virtual void OnTransportWritable(cricket::Transport* transport);
  virtual void OnTransportProxyCandidatesReady(
      cricket::TransportProxy* proxy,
      const cricket::Candidates& candidates);
  virtual void OnCandidatesAllocationDone();

  // Check if a call to SetLocalDescription is acceptable with |action|.
  bool ExpectSetLocalDescription(Action action);
  // Check if a call to SetRemoteDescription is acceptable with |action|.
  bool ExpectSetRemoteDescription(Action action);
  // Creates local session description with audio and video contents.
  bool CreateDefaultLocalDescription();
  // Enables media channels to allow sending of media.
  void EnableChannels();
  // Creates a JsepIceCandidate and adds it to the local session description
  // and notify observers. Called when a new local candidate have been found.
  void ProcessNewLocalCandidate(const std::string& content_name,
                                const cricket::Candidates& candidates);
  // Returns the media index for a local ice candidate given the content name.
  // Returns false if the local session description does not have a media
  // content called  |content_name|.
  bool GetLocalCandidateMediaIndex(const std::string& content_name,
                                   int* sdp_mline_index);
  // Uses all remote candidates in |remote_desc| in this session.
  bool UseCandidatesInSessionDescription(
      const SessionDescriptionInterface* remote_desc);
  // Uses |candidate| in this session.
  bool UseCandidate(const IceCandidateInterface* candidate);
  void RemoveUnusedChannelsAndTransports(
      const cricket::SessionDescription* desc);

  // Allocates media channels based on the |desc|. If |desc| doesn't have
  // the BUNDLE option, this method will disable BUNDLE in PortAllocator.
  // This method will also delete any existing media channels before creating.
  bool CreateChannels(Action action,
                      const cricket::SessionDescription* desc);

  // Helper methods to create media channels.
  bool CreateVoiceChannel(const cricket::SessionDescription* desc);
  bool CreateVideoChannel(const cricket::SessionDescription* desc);
  bool CreateDataChannel(const cricket::SessionDescription* desc);
  // Copy the candidates from |saved_candidates_| to |dest_desc|.
  // The |saved_candidates_| will be cleared after this function call.
  void CopySavedCandidates(SessionDescriptionInterface* dest_desc);

  // Forces |desc->crypto_required| to the appropriate state based on the
  // current security policy, to ensure a failure occurs if there is an error
  // in crypto negotiation.
  // Called when processing the local session description.
  void UpdateSessionDescriptionSecurePolicy(cricket::SessionDescription* desc);

  talk_base::scoped_ptr<cricket::VoiceChannel> voice_channel_;
  talk_base::scoped_ptr<cricket::VideoChannel> video_channel_;
  talk_base::scoped_ptr<cricket::DataChannel> data_channel_;
  cricket::ChannelManager* channel_manager_;
  cricket::TransportDescriptionFactory transport_desc_factory_;
  cricket::MediaSessionDescriptionFactory session_desc_factory_;
  MediaStreamSignaling* mediastream_signaling_;
  IceCandidateObserver * ice_observer_;
  talk_base::scoped_ptr<SessionDescriptionInterface> local_desc_;
  talk_base::scoped_ptr<SessionDescriptionInterface> remote_desc_;
  // Candidates that arrived before the remote description was set.
  std::vector<IceCandidateInterface*> saved_candidates_;
  std::string session_id_;
  uint64 session_version_;
  // If the remote peer is using a older version of implementation.
  bool older_version_remote_peer_;
  // True if this session can create an RTP data engine. This is controlled
  // by the constraint kEnableRtpDataChannels.
  bool allow_rtp_data_engine_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSION_H_
