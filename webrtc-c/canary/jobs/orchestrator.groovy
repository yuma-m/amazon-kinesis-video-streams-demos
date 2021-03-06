import jenkins.model.*

RUNNER_JOB_NAME_PREFIX = "webrtc-canary-runner"
DURATION_IN_SECONDS = 30
MIN_RETRY_DELAY_IN_SECONDS = 60
COLD_STARTUP_DELAY_IN_SECONDS = 60 * 60
GIT_URL = 'https://github.com/aws-samples/amazon-kinesis-video-streams-demos.git'
// TODO: Update this to master
GIT_HASH = 'pr'
COMMON_PARAMS = [
    string(name: 'AWS_KVS_LOG_LEVEL', value: "2"),
    string(name: 'MIN_RETRY_DELAY_IN_SECONDS', value: MIN_RETRY_DELAY_IN_SECONDS.toString()),
    string(name: 'GIT_URL', value: GIT_URL),
    string(name: 'GIT_HASH', value: GIT_HASH),
    string(name: 'LOG_GROUP_NAME', value: "canary"),
]

def cancelJob(jobName) {
    def job = Jenkins.instance.getItemByFullName(jobName)

    echo "Tear down ${jobName}"
    job.setDisabled(true)
    job.getBuilds()
       .findAll({ build -> build.isBuilding() })
       .each({ build -> 
            echo "Kill $build"
            build.doKill()
        })
}

def findRunners() {
    def filterClosure = { item -> item.getDisplayName().startsWith(RUNNER_JOB_NAME_PREFIX) }
    return Jenkins.instance
                    .getAllItems(Job.class)
                    .findAll(filterClosure)
}

NEXT_AVAILABLE_RUNNER = null
ACTIVE_RUNNERS = [] 

pipeline {
    agent {
        label 'master'
    }

    options {
        disableConcurrentBuilds()
    }

    stages {
        stage('Checkout') {
            steps {
                checkout([$class: 'GitSCM', branches: [[name: GIT_HASH ]],
                          userRemoteConfigs: [[url: GIT_URL]]])
            }
        }

        stage('Update runners') {
            /* TODO: Add a conditional step to check if there's any update in webrtc canary
            when {
              changeset 'webrtc-c/canary/**'
            }
            */

            stages {
                stage("Find the next available runner and current active runners") {
                    steps {
                        script {
                            def runners = findRunners()
                            def nextRunner = runners.find({ item -> item.isDisabled() || !item.isBuilding() })

                            if (nextRunner == null) {
                                error "There's no available runner"
                            }

                            NEXT_AVAILABLE_RUNNER = nextRunner.getDisplayName()
                            echo "Found next available runner: ${NEXT_AVAILABLE_RUNNER}"

                            ACTIVE_RUNNERS = runners.findAll({ item -> item != nextRunner && item.isBuilding() })
                                                          .collect({ item -> item.getDisplayName() })
                            echo "Found current active runners: ${ACTIVE_RUNNERS}"
                        }
                    }
                }
            
                stage("Spawn new runners") {
                    steps {
                        script {
                            echo "New runner: ${NEXT_AVAILABLE_RUNNER}"
                            Jenkins.instance.getItemByFullName(NEXT_AVAILABLE_RUNNER).setDisabled(false)
                        }

                        // TODO: Use matrix to spawn runners
                        build(
                            job: NEXT_AVAILABLE_RUNNER,
                            parameters: COMMON_PARAMS + [
                                booleanParam(name: 'USE_TURN', value: true),
                                booleanParam(name: 'TRICKLE_ICE', value: true),
                                string(name: 'DURATION_IN_SECONDS', value: DURATION_IN_SECONDS.toString()),
                                string(name: 'MASTER_NODE_LABEL', value: "personal"),
                                string(name: 'VIEWER_NODE_LABEL', value: "personal"),
                                string(name: 'RUNNER_LABEL', value: "Default"),
                            ],
                            wait: false
                        )
                    }
                }

                stage("Tear down old runners") {
                    when {
                        expression { return ACTIVE_RUNNERS.size() > 0 }
                    }

                    steps {
                        script {
                            try {
                                sleep COLD_STARTUP_DELAY_IN_SECONDS
                            } catch(err) {
                                // rollback the newly spawned runner
                                echo "Rolling back ${NEXT_AVAILABLE_RUNNER}"
                                cancelJob(NEXT_AVAILABLE_RUNNER)
                                throw err
                            }

                            for (def runner in ACTIVE_RUNNERS) {
                                cancelJob(runner)
                            }
                        }
                    }
                }
            }
        }
    }
}
