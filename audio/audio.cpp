#include "audio/audio.hpp"

#include "SDL_mixer.h"
#include "xbak/SoundResource.h"

namespace AudioA {

AudioManager& AudioManager::Get()
{
    static AudioManager audioManager{};
    return audioManager;
}

void AudioManager::ChangeMusicTrack(MusicIndex music)
{
    Logging::LogDebug("AudioManager") << "Changing track to: " << music << "\n";
    if (mCurrentMusicTrack && Mix_PlayingMusicStream(mCurrentMusicTrack))
    {
        Mix_CrossFadeMusicStream(mCurrentMusicTrack, GetMusic(music), -1, 1000, 0);
    }
    else
    {
        Mix_FadeInMusicStream(GetMusic(music), -1, 1000);
    }
    mCurrentMusicTrack = GetMusic(music);
}

void AudioManager::PlaySound(SoundIndex sound)
{
    Logging::LogDebug("AudioManager") << "Playing sound: " << sound << "\n";
    std::visit(overloaded{
        [](Mix_Music* music){
            Mix_PlayMusicStream(music, 1);
            Mix_HookMusicStreamFinished(music, &AudioManager::RewindMusic, nullptr);
        },
        [](Mix_Chunk* chunk){
            
        }},
        GetSound(sound));
}

void AudioManager::RewindMusic(Mix_Music* music, void*)
{
    // This seems to be necessary for some midi snippets that e.g. 61 DRAG
    // that stop playing back after they've been played once or twice...
    //Mix_RewindMusicStream(music);
    Mix_FreeMusic(music);
    auto& soundData = Get().mSoundData;
    soundData.erase(
        std::find_if(
            soundData.begin(),
            soundData.end(),
            [music](const auto& sound)
            {
                return std::holds_alternative<Mix_Music*>(sound.second) 
                    && std::get<Mix_Music*>(sound.second) == music;
            }));
}

void AudioManager::StopMusicTrack()
{
    if (mCurrentMusicTrack)
    {
        Mix_FadeOutMusicStream(mCurrentMusicTrack, 2000);
        mCurrentMusicTrack = nullptr;
    }
}

Mix_Music* AudioManager::GetMusic(MusicIndex music)
{
    if (!mMusicData.contains(music))
    {
        auto& data = SoundResource::GetInstance()->GetSoundData(music.mValue);
        ASSERT(data.sounds.size() > 0);
        auto* fb = data.sounds[0]->GetSamples();
        auto* rwops = SDL_RWFromMem(fb->GetCurrent(), fb->GetSize());
        if (!rwops)
        {
            Logging::LogError("AudioManager") << SDL_GetError() << std::endl;
        }
        Mix_Music* musicData = Mix_LoadMUS_RW(rwops, 0);
        if (!musicData)
        {
            Logging::LogError("AudioManager") << Mix_GetError() << std::endl;
        }

        Mix_SetMusicTempo(musicData, sMusicTempo);

        mMusicData[music] = musicData;
    }

    return mMusicData[music];
}

AudioManager::Sound AudioManager::GetSound(SoundIndex sound)
{
    if (!mSoundData.contains(sound))
    {
        auto& data = SoundResource::GetInstance()->GetSoundData(sound.mValue);
        ASSERT(data.sounds.size() > 0);
        auto* fb = data.sounds[0]->GetSamples();
        auto* rwops = SDL_RWFromMem(fb->GetCurrent(), fb->GetSize());
        if (!rwops)
        {
            Logging::LogError("AudioManager") << SDL_GetError() << std::endl;
        }
        Mix_Music* musicData = Mix_LoadMUS_RW(rwops, 0);
        if (!musicData)
        {
            Logging::LogError("AudioManager") << Mix_GetError() << std::endl;
        }

        Mix_SetMusicTempo(musicData, sMusicTempo);
        mSoundData[sound] = musicData;
    }

    return mSoundData[sound];
}

AudioManager::AudioManager()
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        Logging::LogDebug("Audio") << "Couldn't initialize SDL: "
            << SDL_GetError() << std::endl;
    }

    if (Mix_OpenAudio(sAudioRate, sAudioFormat, sAudioChannels, sAudioBuffers) < 0)
    {
        Logging::LogDebug("Audio") << "Couldn't initialize SDL: "
            << SDL_GetError() << std::endl;
    }

    Mix_VolumeMusic(sAudioVolume);
    Mix_SetMidiPlayer(MIDI_ADLMIDI);
}

void AudioManager::SwitchMidiPlayer(MidiPlayer midiPlayer)
{
    ClearSounds();

    switch (midiPlayer)
    {
    case MidiPlayer::ADLMIDI:
        Mix_SetMidiPlayer(MIDI_ADLMIDI);
        break;
    case MidiPlayer::OPNMIDI:
        Mix_SetMidiPlayer(MIDI_OPNMIDI);
        break;
    case MidiPlayer::FluidSynth:
        Mix_SetMidiPlayer(MIDI_Fluidsynth);
        break;
    default:
        throw std::runtime_error("Invalid midi player type");
    }
}

void AudioManager::ClearSounds()
{
    mCurrentMusicTrack = nullptr;

    for (auto& [_, music] : mMusicData)
    {
        Mix_HaltMusicStream(music);
        Mix_FreeMusic(music);
    }
    mMusicData.clear();

    for (auto& [_, sound] : mSoundData)
    {
        std::visit(overloaded{
            [](Mix_Music* music){
                Mix_HaltMusicStream(music);
                Mix_FreeMusic(music);
            },
            [](Mix_Chunk* chunk){
                Mix_FreeChunk(chunk);
            }},
            sound);
    }
    mSoundData.clear();
}

AudioManager::~AudioManager()
{
    ClearSounds();
    Mix_CloseAudio();
    SDL_Quit();
}

}
