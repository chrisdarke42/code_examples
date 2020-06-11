


To re-deploy the code:

serverless deploy --stage local|dev|qa|prod

If using aws cli profiles:

Profile Creation: aws configure --profile nygb

serverless deploy --stage local|dev|qa|prod --aws-profile nygb

### Virtual Environment - INSTALL

To test and deploy this package, set up a virtual environment. 

`virtualenv venv --python=python3`

### Virtual Environment - RUN

`source venv/bin/activate` (or `source venv/bin/activate.fish` if in fish)

This will kick off the virtual environment with python3 in which to do the
operations.

### Python Libraries

The current serverless config is set up with Python 3.6

`pip install --target ./requirements -r requirements.txt `

### ! Docker is required for Library builds if not in Linux.

There is a config setting to determine if not in linux, use docker for running
the compilation of libraries.

### NPM installed libraries

`npm install serverless-custom-packaging-plugin --save-dev`

## Testing Locally

* Stages: local|dev|qa|prod
* Use aws-profile if aws cli is being used for profiles. 

`serverless invoke local --function debug --stage local --aws-profile nygb`

## Deploying

To deploy code to AWS:

* Stages: local|dev|qa|prod
* Use aws-profile if aws cli is being used for profiles. 

`serverless deploy --stage dev --aws-profile nygb`
