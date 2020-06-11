import json
import os
import io
import re
import sys
import time
import math
from datetime import datetime
# from PIL import Image, ImageDraw
from reportlab.lib import colors
# from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.platypus import Paragraph, Table, TableStyle
from reportlab.lib.enums import TA_CENTER
from xml.sax.saxutils import escape
# import pdfplumber
# import pdfminer.six as pdfminer
from pdfminer.converter import TextConverter
from pdfminer.pdfinterp import PDFPageInterpreter
from pdfminer.pdfinterp import PDFResourceManager
from pdfminer.pdfpage import PDFPage
from pdfminer.layout import LAParams

here = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(here, "vendored"))

from pdfrw import PdfReader, PdfWriter, PageMerge
import boto3
import botocore
import decimal
from botocore.exceptions import ClientError
import PyPDF2 as PyPDF2
from PyPDF2 import PdfFileWriter, PdfFileReader
import requests
from time import sleep

def execPdfParse(event, context):
    # Get page and bucket data from batch
    jsonMessageData = event['Records'][0]['body']
    messageData = json.loads(jsonMessageData)
    s3client = boto3.client('s3')
    paginator = s3client.get_paginator('list_objects')

    # Get requested file
    print("Processing %s key" % messageData['sourceKey'])

    # Now going through all the pages in the batch
    for i in range(len(event['Records'])):
        jsonMessageData = event['Records'][i]['body']
        messageData = json.loads(jsonMessageData)
        bucketName = messageData['bucket']
        # Initialize S3
        s3 = boto3.resource('s3')
        bucket = s3.Bucket(bucketName)

        # Text Output:
        if messageData['filenameText']:
            with io.BytesIO(bucket.Object(messageData['sourceKey']).get()['Body'].read()) as pdf_content:
                inputpdf = PdfFileReader(pdf_content)
                print(f"Creating text from pdf: {messageData['filenameText']}")
                textcontent = extract_text_by_page(pdf_content, messageData['pageNum'])
                bucket.put_object(
                    Key=messageData['filenameText'], Body=textcontent)

        # Data for specific file.
        with io.BytesIO(bucket.Object(messageData['sourceKey']).get()['Body'].read()) as pdf_content:
            inputpdf = PdfFileReader(pdf_content)
            pdfOutput = PdfFileWriter()
            pdfPage = inputpdf.getPage(messageData['pageNum'])

            # Create PDF file from the single page
            pdfOutput.addPage(pdfPage)
            # Store in temp location @todo: Get ByteIO working here
            tmp_filename = '/tmp/' + messageData['filenamePDF'].replace("/", "-")
            with open(tmp_filename, 'wb') as outputStream:
                pdfOutput.write(outputStream)
                print(f"Creating PDF page in tmp location {tmp_filename}")
            # Push file up to final location
            bucket.upload_file(tmp_filename, messageData['filenamePDF'])
            print(f"Uploading {messageData['filenamePDF']} to bucket {bucketName}")

        fileCount = 0
        for result in paginator.paginate(Bucket=bucketName, Prefix=messageData['imageDir']):
            contents = result.get("Contents")
            if contents:
                fileCount += len(contents)
            else:
                print(f"Error paginating on {messageData['imageDir']}")

        if fileCount == messageData['totalPages']:
            try:
                # Delete lock object
                print(f"Deleting lock file {messageData['lockKey']}")
                bucket.Object(messageData['lockKey']).delete()
                # Add file to the queue for indexing in Elastic
                # DISABLING THIS TILL ITS NEEDED
                # indexMessage = {}
                # indexMessage["sourceKey"] = messageData['sourceKey']
                # indexMessage["parsedDir"] = messageData['parsedDir']
                # indexMessage["totalPages"] = messageData['totalPages']
                # jsonMessage = json.dumps(indexMessage)
                # sqsclient = boto3.client('sqs')
                # response = sqsclient.send_message(
                #     QueueUrl='https://sqs.us-east-1.amazonaws.com/531174535259/nygb-ready-to-index',
                #     MessageBody=jsonMessage)
                # print('Completed task, lock file can be removed, and queue updated')
            except:
                print('Lock file no longer exists. Process has completed.')
        else:
            print('Pages processed did not match File Count.')
            print(f"File Count: {fileCount}, totalPages: {messageData['totalPages']}")

def initPdfParse(event, context):
    print('initPDFParse list: ')
    print(event['Records'])
    sourceKey = event['Records'][0]['s3']['object']['key']
    bucketName = event['Records'][0]['s3']['bucket']['name']
    print(f"Initializing PDF Parsing: {sourceKey}")
    s3 = boto3.resource('s3')
    object = s3.Object(bucketName, sourceKey)
    if "copyright" in object.metadata:
        print(f"Existing Copyright: {object.metadata['copyright']}")
        status = pdfParse(sourceKey, bucketName)
    else:
        print('No Copyright Applied yet')
        status = copyrightParse(sourceKey, bucketName, context)
    # print(f'initPdfParse: {status}')
    return {'message' : status }

def copyrightParse(sourceKey, bucketName, context):
    # BOTO3 objects
    s3 = boto3.resource('s3')
    s3client = boto3.client('s3')
    object = s3.Object(bucketName, sourceKey)

    # Copyright Data
    metadata = object.metadata
    if "copyright" in metadata:
        return 'Copyright already exists - aborting'
    dateTimeObj = datetime.now()
    timestampStr = dateTimeObj.strftime("%d-%b-%Y (%H:%M:%S.%f)")
    metadata['copyright'] = timestampStr

    # Get prelim data from object
    with io.BytesIO(object.get()['Body'].read()) as pdf_content_sample:
        existing_pdf = PdfReader(pdf_content_sample)
        # Get Dimensions of document to make corresponding sized watermark
        mbox = existing_pdf.pages[0].MediaBox
        mediabox = tuple(float(x) for x in mbox)

        ### ReportLab implementation
        # Get Source PDF to watermark - Load single page to generate watermark to the right size
        # Create memory position for Watermark PDF
        with io.BytesIO() as packet:
            print('Loading PDF file - Watermark generation')
            height = 40
            width = mediabox[2]

            # create a new PDF with Reportlab
            can = canvas.Canvas(packet)
            can.setPageSize((width, height))

            # Get Copyright content
            copyrightContent = getCopyrightContent()

            # Stylesheet additions
            stylesheet = getSampleStyleSheet()
            style_watermark = stylesheet["Normal"]
            style_watermark.alignment = TA_CENTER
            style_watermark.textColor = colors.Color(0, 0, 0, alpha=0.5)
            style_watermark.fontSize = 8
            style_watermark.font = 'Helvetica'
            # Creating Paragraph
            copyright_paragraph = Paragraph(copyrightContent, style_watermark)
            # Creating Table to wrap Paragraph
            data = [[copyright_paragraph]]
            table = Table(data)
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, -1), colors.Color(255, 255, 255, alpha=0.5)),
            ]))
            # Adding Table to Canvas
            # Make sure the width is an integer!
            print(f'Table width set to {math.floor(width)}')
            table.wrapOn(can, math.floor(width), 15)
            table.drawOn(can, 0, 0)
            # Saving
            can.save()
            # Move to start of memory pointer
            packet.seek(0)

            watermark_input = PdfReader(packet)
            watermark = watermark_input.pages[0]
            # Iterate through pages, updating source file.
            for current_page in range(len(existing_pdf.pages)):
                merger = PageMerge(existing_pdf.pages[current_page])
                merger.add(watermark).render()
            # write the modified content to disk
            writer_output = PdfWriter()
            outputStream = io.BytesIO()
            with outputStream as pdfOutput:
                writer_output.write(pdfOutput, existing_pdf)
                print('File written to PDFWriter')
                pdfOutput.seek(0)
                s3client.upload_fileobj(pdfOutput, bucketName, sourceKey, ExtraArgs={"Metadata": metadata})
            status = f'Copyright Set: {timestampStr}'
    return status

def pdfCleanup(event, context):
    message = 'Delete Process Started'
    sourceKey = event['Records'][0]['s3']['object']['key']
    bucketName = event['Records'][0]['s3']['bucket']['name']
    try:
        # Initialize S3
        s3 = boto3.resource('s3')
        bucket = s3.Bucket(bucketName)
        # Get the parts of the path we need for processing
        paths = pathSections(sourceKey)
        # Delete pdf pages
        bucket.objects.filter(Prefix=paths["new_dir"]).delete()
        # Delete txt files
        bucket.objects.filter(Prefix=paths["new_dir_parsed"]).delete()
        # Delete any stray lock file
        bucket.Object(paths["lock_key"]).delete()
        # Add some output
        message = f'All files related to {sourceKey} deleted'
    except KeyError:
        message = 'No file defined'
    except botocore.exceptions.ClientError as e:
        if e.response['Error']['Code'] == "NoSuchKey":
            message = 'Key does not exist'
    print(f'PDF Cleanup: {message}')

def pathSections(sourceKey):
    # Get the sections of the path
    pathparts = os.path.splitext(sourceKey)
    dir = pathparts[0]
    pathsplit = dir.split('/')
    pagesPath = pathsplit[:]
    pagesPath[0] = 'pages'
    textPath = pathsplit[:]
    textPath[0] = 'text'
    tmpPath = pathsplit[:]
    tmpPath[0] = 'tmp'
    bundlePath = pathsplit[:]
    bundlePath[0] = 'bundle'
    filename = pathsplit[-1]
    lockKey = pathparts[0] + '.lock'
    # Define new directories
    pathSections = {}
    pathSections["lock_key"] = lockKey
    pathSections["new_dir"] = '/'.join(pagesPath) + '/'
    pathSections["new_dir_parsed"] = '/'.join(textPath) + '/'
    pathSections["tmp_dir"] = '/' . join(tmpPath) + '/'
    pathSections["bundle_dir"] = '/'.join(bundlePath) + '/'
    pathSections["filename"] = filename
    return pathSections

def pdfParse(sourceKey, bucketName):
    message = 'No Result'
    try:
        # Initialize S3
        s3 = boto3.resource('s3')
        bucket = s3.Bucket(bucketName)
        s3_file_object = bucket.Object(sourceKey)

        # Get requested file
        pdf_file = s3_file_object.get()['Body'].read()
        pdf_content = io.BytesIO(pdf_file)
        inputpdf = PdfFileReader(pdf_content)

        # Get the parts of the path we need for processing
        paths = pathSections(sourceKey)

        # Set up lock file
        lockData = {}
        lockData['status'] = 'in_progress'
        bucket.put_object(Key=paths["lock_key"], Body=json.dumps(lockData))

        clientSQS = boto3.resource('sqs')

        renderText = True
        matchObject = re.match('.*/image/.*', sourceKey)
        if matchObject:
           renderText = False

        sqs_name = os.environ.get('sqs_parse_name')

        for i in range(inputpdf.numPages):
            # Set up params for Queue
            # File name page numbers are to be 1-based.

            # Check if text filename should be set. If its false then the parser wont do text, just pdf.
            new_filename_text = False
            if renderText:
                new_filename_text = paths["new_dir_parsed"] + paths["filename"] + "-%s.txt" % (i + 1)

            # PDF path.
            new_filename_pdf = paths["new_dir"] + paths["filename"] + "-%s.pdf" % (i + 1)

            # Set up queue message
            pageMessage = {}
            pageMessage["sourceKey"] = sourceKey
            pageMessage["bucket"] = bucketName
            # pageNum is only used for index management, 0-based.
            pageMessage["pageNum"] = i
            pageMessage["lockKey"] = paths["lock_key"]
            pageMessage["parsedDir"] = paths["new_dir_parsed"]
            pageMessage["imageDir"] = paths["new_dir"]
            pageMessage["totalPages"] = inputpdf.numPages
            pageMessage["filenameText"] = new_filename_text
            pageMessage["filenamePDF"] = new_filename_pdf
            jsonMessage = json.dumps(pageMessage)

            # Add message to queue
            print(f'SQS: Key {sourceKey} - page {i} added to {sqs_name}')
            queue = clientSQS.get_queue_by_name(QueueName=sqs_name)
            response = queue.send_message(MessageBody=jsonMessage)

        message = f'Processed {i + 1} pages'
    except KeyError:
        message = 'No file defined'
    except botocore.exceptions.ClientError as e:
        if e.response['Error']['Code'] == "NoSuchKey":
            message = 'Key does not exist'
    return message

def getCopyrightContent():
    # json.load(urllib2.urlopen("url"))
    sourceDomain = os.environ.get('DOMAIN')
    print(f'Connecting to {sourceDomain}')
    response = requests.get(f'{sourceDomain}/api/v1/NPcopyright.json')
    data = response.json()
    copyright_text = data['text']
    print(f'copyright response: {copyright_text}')
    return escape(copyright_text)

def extract_text_by_page(page_byte_data, page_num):
    rsrcmgr = PDFResourceManager()
    retstr = io.StringIO()
    codec = 'utf-8'
    laparams = LAParams()
    device = TextConverter(rsrcmgr, retstr, codec=codec, laparams=laparams)
    interpreter = PDFPageInterpreter(rsrcmgr, device)
    password = ""
    maxpages = 0
    caching = True
    pagenos = set()

    with page_byte_data as fh:
        pcount = 0
        for page in PDFPage.get_pages(fh, pagenos, maxpages=maxpages, password=password,caching=caching, check_extractable=True):
            if (pcount == page_num):
                interpreter.process_page(page)
                text = retstr.getvalue()
                return text
            pcount = pcount + 1
    # close open handles
    device.close()
    rsrcmgr.close()
    return ''

# Test Callback function.
def debug(event, context):

    # Get Source PDF to watermark
    filename = "sample.pdf"
    existing_pdf = PdfReader(open(filename, "rb"))

    # Get Dimensions of document to make corresponding sized watermark
    mbox = existing_pdf.pages[0].MediaBox
    mediabox = tuple(float(x) for x in mbox)

    with io.BytesIO() as packet:
        height = 40
        width = mediabox[2]
        # create a new PDF with Reportlab
        can = canvas.Canvas(packet)
        can.setPageSize((width, height))

        # Get Copyright content
        copyrightContent = getCopyrightContent()

        # Stylesheet additions
        stylesheet = getSampleStyleSheet()
        style_watermark = stylesheet["Normal"]
        style_watermark.alignment = TA_CENTER
        style_watermark.textColor = colors.Color(0, 0, 0, alpha=0.5)
        style_watermark.fontSize = 8
        style_watermark.font = 'Helvetica'
        # Creating Paragraph
        copyright_paragraph = Paragraph(copyrightContent, style_watermark)
        # Creating Table to wrap Paragraph
        data = [[copyright_paragraph]]
        table = Table(data)
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, -1), colors.Color(255, 255, 255, alpha=0.5)),
        ]))
        # Adding Table to Canvas
        table.wrapOn(can, math.floor(width), 15)
        table.drawOn(can, 0, 0)
        # Saving
        can.save()
        # Move to start of memory pointer
        packet.seek(0)

        # Setting up PDF as a PDFFileReader object
        watermark_input = PdfReader(packet)
        watermark = watermark_input.pages[0]
        # Iterate through pages, updating source file.
        for current_page in range(len(existing_pdf.pages)):
            print(f'page {current_page}')
            merger = PageMerge(existing_pdf.pages[current_page])
            merger.add(watermark).render()

        # write the modified content to disk
        writer_output = PdfWriter()
        outputStream = open(f"processed_{filename}", "wb")

        with outputStream as pdfOutput:
            writer_output.write(pdfOutput, existing_pdf)

        print('Processed PDF - copyright added')

# Catch all for running 'main' function when handler file is called without arguments for testing.
if __name__ == "__main__":
    debug('', '')